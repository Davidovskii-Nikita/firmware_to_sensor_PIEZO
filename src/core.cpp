#include <Arduino.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "by.pool.ntp.org");     // выбор сервера NTP("by.pool.ntp.org"), cмещение пояса(10800)

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

double get_time(double s_t)
{ 
  return  ((double)millis())/1000 + s_t;
}

