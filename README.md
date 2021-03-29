## Прошивка для датчика вибрации на основе пьезоэлемента

В основе, лежит пьезоэлемент. Для получения температуры используется датчик DS18B20.
Управляющем устройством выступает чип esp8266 на борту модуля ESP-12.    
Вся прошивка написана с помощью платформы [PlatformIO](https://platformio.org/) на базе VSC, фреймворк Arduino.
 
### Содержание:
* Как работает код.
* Схема подключения.
* Отладка.
* Возможные проблемы.
---
### Как рабоатет код.
Прошивка состоит из 3-x файлов:
* **core.h** 
Header-файл. Содержит инициализацию библиотек, переменных и функций. 
* **core.cpp** 
Содержит вспомошательные функции.
* **main.cpp**
Содержит основной код.   
После подключения основного header-файла в функции setup объявляется использование глобальных переменных, начинается подключение по WiFi.   
Дальше идет большой кусок кода, нужный для возможности прошивки устройства через WiFi:   
``` C
if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    MDNS.begin(host_OTA);
    server.on("/", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });
    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Ticker_A.detach();
        Ticker_V.detach();
        Ticker_T.detach();
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });
```    
Для изменения внешнего вида web-страницы, можно менять HTML-код, написанный в переменной *serverIndex*, объявленной в файле core.h.     
Далее инициализируется MDNS. Вызывается функция **update_ntp** для получения времени с NTP серверов. 
``` C
double update_ntp() 
{
    double s_t; 
    timeClient.begin(); 
    delay(100);
    timeClient.update();
    s_t = timeClient.getEpochTime();
    if ( s_t > 2000000000 || s_t < 15000.0)
        {
        update_ntp(); // рекурсия этой функции пока не получим "нормальное" время
        }
    return s_t;
}
```    
В дальнейшем, для получения точного времени вызывается функция *get_time*, которая добовляет к полученному выше числу *millis()* - колличество миллисекунд, прошедших после включения ESP.   
Для работы с датчиком температуры в фауле *core.h* инициализируется One-Wire шина.
```C
#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DS18B20 sensor(&oneWire);
```
В *setup()* файла *main.cpp* создается экземпляр, и выставляется разрешение:
```C
sensor.begin();
sensor.setResolution(9);// 9 минимальная
sensor.requestTemperatures();
```
Далее идет блок инициализации таймеров:
``` C
Ticker_V.attach_ms(period_v,get_vibrospeed);
Ticker_A.attach_ms(period_a,upate_vibrospeed_value);
Ticker_T.attach_ms(period_temp,update_temperature_value);
```   
Время работы таймеров определяется в файле *core.h*.   
В функции *get_vibrospeed* получаются значения ускорения путем интегрирования виброскорости, получаемого с пьезоэлемента. 
```C
void get_vibrospeed()
{
    extern float vibro_speed;
    extern float current_accel;
    extern float old_accel;
    current_accel = analogRead(A0);
    vibro_speed = vibro_speed + ((current_accel+old_accel)*(g/200.0));
    old_accel = current_accel;
}
```    
В функции *update_vibrospeed_value* заполняются глобальные массивы *opros_axel* и *opros_axel_time* добавлением глобальной переменной *vibro_speed* и вызовом функции *get_time* соответственно.   
```C
double get_time(double s_t)
{ 
  return  ((double)millis())/1000 + s_t;
}
```   
А так же обновлятся глобальный флаг *flag_a* если колличество записываемых в массив значений больше переменной *range_a*(задается в файле *core.h*) и обнуляeтся переменная виброскорости (*vibro_speed*).   
Функция *update_temperature_value*, делает тоже самое, однако вместо значений виброскорости записываются значения температуры из функции *getTempС*(встроенная).
```C
float T;
T = sensor.getTempC();
temp_to_json=String(T);
```    
В функции *loop()* постоянно обновляется содержимое web-сервера и MDNS а так же проверяются флаги, сигнализирующие о наполненности массивов.
```C
void loop() 
{
  // Отображение страницы для OTA-update
  server.handleClient();
  MDNS.update();
  extern bool flag_a, flag_temp;
  extern uint16_t count_temp, count_a;
  if(flag_a && flag_temp)
  {
    post_json(); // сбор и отправка данных
    count_temp = 0; // обнуление всех счетчиков и флагов для следующей иттерации
    count_a = 0;
    flag_a = false;
    flag_temp = false;
  }
 
}
```    
Если массивы заполнены, обнуляются глобальные счетчики *count_a* и *count_temp*, сбрасываются флаги и вызывается функция post_json(). В которой создается json-файл, заполняется данными из глобальных массивов и отправляется POST-запросом в адреса *URL1* и *URL2*.   
Так каждые 25 секунд отправляется json-файл, содержащий 100 значений виброскорости и 10 значений температуры со своими временными ветками в UNIX-формате включая миллисекунды. Каждые 25 миллисекунд собирается и интегрируются значения ускорения.    
   
### Схема подключения.   
В общем виде схема подключения выглядит следующим образом:
![Сехма устройства](https://github.com/Davidovskii-Nikita/firmware_to_sensor_PIEZO/blob/master/dosc/%D0%9F%D1%80%D0%B8%D0%BD%D1%86%D0%B8%D0%BF%D0%B8%D0%B0%D0%BB%D1%8C%D0%BD%D0%B0%D1%8F%20%D1%81%D1%85%D0%B5%D0%BC%D0%B0%20%D0%BF%D1%8C%D0%B5%D0%B7%D0%BE.png)   
    
### Отладка  
Для настройки работы прошивки можно изменить переменные содержащиеся в файле *core.h* блок настройки пользователя.   
```C
const char* ssid = "Davidovskii";// название WiFi сети
const char* password = "4054414LabU" ;// пароль WiFi сети
const int full_scale_range = 16; // диапазон измерений акселерометра( 2, 4, 8, 16)
const uint16_t period_a = 250; // Частота записи виброскорости
const uint16_t period_temp = 2500; // Частота записи темперартуры
const uint16_t period_v = 25; // Частота итегрирования виброускрорения
// const uint16_t period_c = 200; // Частота калибрования 
const uint16_t range_a = 100; // колличество значений виброскорости в 1 пакете
const uint16_t range_temp = 10; // колличество значений времени в 1 пакете
// const char* host = "http://192.168.1.212:8001"; // адрес хоста
const char* host = "192.168.43.52:8001"; // адрес хоста
String URL1 = "http://95.215.204.182:40001"; // адрес куда отправляются POST запросы
// String URL2 = "http://192.168.1.212:8001/nkvm"; // адрес куда отправляются POST запросы
String URL2 = "http://192.168.43.52:8001/nkvm"; // адрес куда отправляются POST запросы
```   
Для настройки чувстительности датчика температуры так же необходимо изменить передаваемую в функции константу:
```C
sensor.setResolution(9);// 9 минимальная
```
   
### Возможные проблемы.
* Не проверялся максимальный размер json-файла, возможный для передачи за одну итерацию.
* Не проверялась точность получаемого времени за большой промежуток времени (масимум 1-2 часа).
* Отсутствует статическая характеристика пьезоэлемента, значения получаются в "попугаях".

