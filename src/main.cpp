#include <Arduino.h>
#include "core.h"

void setup() 
{
  extern double sync_time;
  extern String MAC;
  WiFi.begin(ssid, password);
  delay(100);
  WiFi.mode(WIFI_AP_STA); 
  // Создание WEB-страницы
  //====================================================================================
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
    server.begin();
    MDNS.addService("http", "tcp", 80);

    Serial.printf("Ready! Open http://%s.local in your browser\n", host_OTA);
  } 
  //====================================================================================
  MDNS.begin(host_OTA);
  MDNS.addService("http", "tcp", 80); 

  Serial.begin(9600); // отладка по последовательному порту
  Wire.begin(2, 0);  // инициализация I2C на GPIO 2 и 0 (SDA, SCL)
  sync_time = update_ntp(); // получение UNIX-времени
  //====================================================================================
  MAC = WiFi.macAddress(); // Получение MAC адреса устройства
  capacity = 2 * JSON_ARRAY_SIZE(range_temp) + 2 * JSON_ARRAY_SIZE(range_a) + JSON_OBJECT_SIZE(5) + 3500; // вычисление объема JSON файла
  local_time_ms = millis();

  sensor.begin();
  sensor.setResolution(9);// 9 минимальная
  sensor.requestTemperatures();

  WiFi.setOutputPower(5);

  // Блок инициализации таймеров
  //====================================================================================
  Ticker_V.attach_ms(period_v,get_vibrospeed);
  Ticker_A.attach_ms(period_a,upate_vibrospeed_value);
  Ticker_T.attach_ms(period_temp,update_temperature_value);
  //====================================================================================
}

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

void get_vibrospeed()
{
    extern float vibro_speed;
    extern float current_accel;
    extern float old_accel;
    current_accel = analogRead(A0);
    vibro_speed = vibro_speed + ((current_accel+old_accel)*(g/200.0));
    old_accel = current_accel;
  
  
}
 
void upate_vibrospeed_value()
{
  extern bool flag_a;
  extern uint16_t count_a;
  String time_to_json;
  String speed_to_json;
  if (count_a<range_a)
  {
    time_to_json = String(get_time(sync_time));
    speed_to_json =String(vibro_speed);
    opros_axel[count_a]=speed_to_json; // запись виброскорости и времени в массивы
    opros_axel_time[count_a]=time_to_json;
    ++count_a;
    flag_a = false;
    vibro_speed = 0;
  }
  else
  {
    flag_a = true;
    vibro_speed = 0;
  }

}

void update_temperature_value()
{
  extern bool flag_temp;
  extern uint16_t count_temp;
  String time_to_json;
  String temp_to_json;
  if(count_temp<range_temp)
  {
    float T;
    T = sensor.getTempC();
    time_to_json = String(get_time(sync_time));
    temp_to_json=String(T);
    opros_temp[count_temp]=temp_to_json;// запись температуры и времени в массив
    opros_temp_time[count_temp]=time_to_json;
    ++count_temp;
    flag_temp = false;
  }
  else
  {
    flag_temp = true;
  }
}

void post_json()
{
  WiFiClient client;
  HTTPClient http;
  String buffer; // локальны буффер json документа
  DynamicJsonDocument jsonDocument(capacity); // объявление динамического jsom документа
  jsonDocument ["MAC"] = MAC; // добавление MAC адресаа в JSON
  // добавление соответвствующих массивов в json
  JsonArray Axel_time = jsonDocument.createNestedArray("Axel_time");
  JsonArray Axel = jsonDocument.createNestedArray("Axel");
  JsonArray Temp_time = jsonDocument.createNestedArray("Temp_time");
  JsonArray Temp = jsonDocument.createNestedArray("Temp");

  int i = 0;
  int j = 0;
  for(i = 0; i < range_temp; i++)// наполнеие массивов данными
  {
    Temp.add(opros_temp[i]);
    Temp_time.add(opros_temp_time[i]);
  }
  for(j=0; j<range_a; j++)// наполнеие массивов данными
  {
    Axel.add(opros_axel[j]);
    Axel_time.add(opros_axel_time[j]);
  }
  
  serializeJson(jsonDocument, buffer); // создание заполненного json документа

  http.begin(client, URL1);// отправка
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(buffer);
  http.end();

  http.begin(client, URL2);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(buffer);
  http.end();
}

