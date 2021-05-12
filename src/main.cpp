#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <BH1750.h>
#include <BME280I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Debug.h>
#include <Credentials.h>

#define SNZ_PWR_PIN 19
#define BATT_LVL_PIN 33
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 10
#define DATA_DIFF_TO_SEND 0.5
// RTC_DATA_ATTR int bootCount = 0;
const int PORT = 8080;
const int IDX_tb = 2;
const int IDX_lux = 3;
// int batteryLevel = 50;
// int rssiLevel = 0;
unsigned long previousMillis = millis();
unsigned long startMillis = 0;
float temp, hum, bar, lux;
int battLevel, rssiLevel;
BH1750 lightMeter;
BME280I2C bme;
HTTPClient http;

// RTC_DATA_ATTR int bootCount;
RTC_DATA_ATTR float tempOld;
RTC_DATA_ATTR float barOld;
RTC_DATA_ATTR float luxOld;

void goToSleep()
{
    digitalWrite(SNZ_PWR_PIN, LOW);
    //client.disconnect();
    //------->yield();
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    //esp_wifi_stop();
    //ESP.deepSleep(deep_sleep, WAKE_RF_DEFAULT);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

    // DEBUG_PRINTLN("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    // Serial.flush();
    DEBUG_PRINTLN("About to go to sleep, time taken to complete cycle: " + String(millis() - startMillis));
    Serial.flush(); 
    esp_deep_sleep_start();
    // delay(250);
}

void setupWifi()
{
    DEBUG_PRINTLN("Connecting to " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        // delay(250);
        // Serial.print(".");
        if (millis() - startMillis > 10000)
        {
            DEBUG_PRINTLN("Taken too long to connect to WiFi. Going to sleep");
            goToSleep();
        }
    }
    
    rssiLevel = map(WiFi.RSSI(), -98, -50, 0, 10);
    // DEBUG_PRINTLN("RSSI: " + String(rssiLevel));
    DEBUG_PRINTLN("WiFi connected, RSSI: " + String(rssiLevel));
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

void getMeasurements()
{
    bme.read(bar, temp, hum);
    lux = lightMeter.readLightLevel();
    battLevel = map(analogRead(BATT_LVL_PIN), 2730, 3475, 0, 100);
    DEBUG_PRINTLN("Temp: " + String(temp) + " °C, Pressure: " + String(bar) + " hPa, Light: " + String(lux) + " lx, Batt: " + String(battLevel) + " %");
}

bool hasDataChanged()
{
    DEBUG_PRINTLN("Temp: " + String(tempOld));
    DEBUG_PRINTLN("Bar: " + String(barOld));
    DEBUG_PRINTLN("Lux: " + String(luxOld));
    if ((abs(temp - tempOld) > DATA_DIFF_TO_SEND) || (abs(bar - barOld) > DATA_DIFF_TO_SEND) || (abs(lux - luxOld) > DATA_DIFF_TO_SEND))
    {
        tempOld = temp;
        barOld = bar;
        luxOld = lux;
        DEBUG_PRINTLN("Temp new: " + String(tempOld));
        DEBUG_PRINTLN("Bar new: " + String(barOld));
        DEBUG_PRINTLN("Lux new: " + String(luxOld));
        return true;
    }
    else return false;
}

void transmitData()
{
    if (hasDataChanged())
    {
        setupWifi();
        //json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=TEMP;BAR;BAR_FOR;ALTITUDE
        String url = "/json.htm?type=command&param=udevice&rssi=" + String(constrain(rssiLevel, 0, 12)) + "&battery=" + String(constrain(battLevel, 0, 100)) + "&idx=" + String(IDX_tb) + "&nvalue=0&svalue=" + String(constrain(temp, 0, 100)) + ";" + String(constrain(bar, 0, 2000)) + ";0;0";
        sendToServer(url);
        //json.htm?type=command&param=udevice&idx=IDX&svalue=VALUE
        url = "/json.htm?type=command&param=udevice&rssi=" + String(constrain(rssiLevel, 0, 12)) + "&battery=" + String(constrain(battLevel, 0, 100)) + "&idx=" + String(IDX_lux) + "&nvalue=0&svalue=" + String(constrain(lux, 0, 10000));
        sendToServer(url);
    }
}

void init()
{
    startMillis = millis();
    pinMode(SNZ_PWR_PIN, OUTPUT);
    digitalWrite(SNZ_PWR_PIN, HIGH);
    Serial.begin(9600);
    Wire.begin();
    bme.begin();
    lightMeter.begin();
}

void setup()
{

    // while(1)
    // {
    //     DEBUG_PRINTLN(analogRead(BATT_LVL_PIN));
    // }
    init();
    getMeasurements();
    transmitData();
    
    // while (!Serial)
    // bootCount++;
    // DEBUG_PRINTLN("Boot number: " + String(bootCount));
    // delay(10);

    // delay(100);

    // lightMeter.setMTreg(32);
    // lightMeter.begin();
    // BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    // BME280::PresUnit presUnit(BME280::PresUnit_Pa);
    // DEBUG_PRINTLN("Setup successful");

    goToSleep();
}

void loop()
{
}