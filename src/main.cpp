#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <BH1750.h>
#include <BME280I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Debug.h>

#define SNZ_PWR_PIN 19
#define BATT_LVL_PIN 33
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 300
// RTC_DATA_ATTR int bootCount = 0;
const char *WIFI_SSID = "WLAN14510355";
const char *WIFI_PASSWORD = "Mtm6nvcwtxtn";
const char *HOST = "192.168.0.180";
const int PORT = 8080;
const int IDX_tb = 2;
const int IDX_lux = 3;
// int batteryLevel = 50;
int rssiLevel = 50;
unsigned long previousMillis = millis();
unsigned long startMillis = 0;
BH1750 lightMeter;
BME280I2C bme;
HTTPClient http;

void goingToSleep()
{
    digitalWrite(SNZ_PWR_PIN, LOW);
    DEBUG_PRINTLN("About to go to sleep: Time taken to complete cycle: " + String(millis() - startMillis));
    //client.disconnect();
    //------->yield();
    //WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    //esp_wifi_stop();
    //ESP.deepSleep(deep_sleep, WAKE_RF_DEFAULT);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    // DEBUG_PRINTLN("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    Serial.flush();
    esp_deep_sleep_start();
    delay(250);
}

void setup_wifi()
{
    DEBUG_PRINTLN("Connecting to " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if (millis() - startMillis > 15000)
        {
            DEBUG_PRINTLN("Taken too long to connect to WiFi. Going to sleep");
            goingToSleep();
        }
    }
    
    rssiLevel = map(WiFi.RSSI(), -98, -50, 0, 10);
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("RSSI: " + String(rssiLevel));
    DEBUG_PRINTLN("WiFi connected!");
}

void sendToServer(String url)
{
    // Serial.print("connecting to ");
    // DEBUG_PRINTLN(HOST);
    // Serial.print("Requesting URL: ");
    // DEBUG_PRINTLN(url);
    http.begin(HOST, PORT, url);
    int httpCode = http.GET();
    if (httpCode)
    {
        if (httpCode == 200)
        {
            String payload = http.getString();
            // DEBUG_PRINTLN("Domoticz response ");
            DEBUG_PRINTLN(payload);
        }
    }
    // DEBUG_PRINTLN("closing connection");
    http.end();
}

void setup()
{
    startMillis = millis();
    Serial.begin(9600);
    // while(1)
    // {
    //     DEBUG_PRINTLN(analogRead(BATT_LVL_PIN));
    // }
    setup_wifi();
    // while (!Serial)
    // bootCount++;
    // DEBUG_PRINTLN("Boot number: " + String(bootCount));
    // delay(10);
    pinMode(SNZ_PWR_PIN, OUTPUT);
    digitalWrite(SNZ_PWR_PIN, HIGH);
    delay(100);
    Wire.begin();
    bme.begin();
    lightMeter.begin();
    // BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    // BME280::PresUnit presUnit(BME280::PresUnit_Pa);
    DEBUG_PRINTLN("Setup successful");
}

void loop()
{
    float temp, hum, bar;
    bme.read(bar, temp, hum);
    float lux = lightMeter.readLightLevel();
    int battLevel = map(analogRead(BATT_LVL_PIN), 2730, 3475, 0, 100);
    DEBUG_PRINTLN("Temp: " + String(temp) + " °C, Pressure: " + String(bar) + " hPa, Light: " + String(lux) + " lx, Batt: " + String(battLevel) + " %");
    delay(10000);
    //json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=TEMP;BAR;BAR_FOR;ALTITUDE
    String url = "/json.htm?type=command&param=udevice&rssi=" + String(rssiLevel) + "&battery=" + String(battLevel) + "&idx=" + String(IDX_tb) + "&nvalue=0&svalue=" + String(temp) + ";" + String(bar) + ";0;0";
    sendToServer(url);
    //json.htm?type=command&param=udevice&idx=IDX&svalue=VALUE
    url = "/json.htm?type=command&param=udevice&rssi=" + String(rssiLevel) + "&battery=" + String(battLevel) + "&idx=" + String(IDX_lux) + "&nvalue=0&svalue=" + String(lux);
    sendToServer(url);
    goingToSleep();
}