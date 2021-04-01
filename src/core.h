#ifndef CORE_H
#define CORE_H
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
#include <OneWire.h>
#include <DS18B20.h>
// Блок настройки пользователем
//====================================================================================
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
//====================================================================================
// Функциональные переменные
//====================================================================================
String MAC; // MAC адресс устройсва
size_t capacity; // объем Json 
const float g = 9.81; // постоянная ускорения
String opros_axel [range_a]; // значения ускорения для json
String opros_axel_time [range_a];// значения времения по ускорения для json
String opros_temp [range_temp]; // значения температуры для json
String opros_temp_time [range_temp]; // значения времени по температуре для json
uint16_t count_a,count_temp; // счетчики точек температуры и вибросокорости
bool flag_a, flag_temp; // флаги достижения счетчиками range_a range_temp
uint16_t local_time_ms;
double sync_time; // глобальная переменная, содержащая UNIX время в момент старта ESP
const char* host_OTA = "esp-8266";// название устройства в локальной сети для прошивки через браузер
const char* serverIndex = "<title>Update ESP</title><h1> Update ESP12  </h1><img src = ""https://raw.githubusercontent.com/AchimPieters/ESP8266-12F---Power-Mode/master/ESP8266_12X.jpg""><form method='POST' action='/update' enctype='multipart/form-data'> <input type='file' name='update'><input type='submit' value='Update'></form>";
const char* update_path = "/firmware";
float vibro_speed;
float current_accel;
float old_accel;
double  offset_startup_time;
//====================================================================================
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
Ticker Ticker_A, Ticker_T, Ticker_V; // инициализация счетчиков

#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DS18B20 sensor(&oneWire);
// Инициализация функций
//====================================================================================
double update_ntp(); // функция получения времени
double get_time(double s_t, double offset); // функция синхронизации времени s_t - время UNIX (sync_time)
void get_vibrospeed();//интешрирование ускорения, управляется таймером Ticker_V
void upate_vibrospeed_value(); //запись значения виброскорости в массив с меткой времени. Управляется таймером Ticker_A
void update_temperature_value(); // запись значения температуры в массив с меткой времени. Управляется таймером Ticker_T
void post_json(); // создает и отправляет json документ
//====================================================================================
#endif