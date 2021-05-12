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
#define TIME_TO_SLEEP 600
const int PORT = 8080;
const int IDX_tb = 2;
const int IDX_lux = 3;
unsigned long previousMillis = millis();
unsigned long startMillis = 0;
float temp, hum, bar, lux;
int battLevel, rssiLevel;
BH1750 lightMeter;
BME280I2C bme;
HTTPClient http;

RTC_DATA_ATTR float tempOld;
RTC_DATA_ATTR float barOld;
RTC_DATA_ATTR float luxOld;

void goToSleep()
{
    digitalWrite(SNZ_PWR_PIN, LOW);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    DEBUG_PRINTLN("About to go to sleep, time taken to complete cycle: " + String(millis() - startMillis));
    Serial.flush(); 
    esp_deep_sleep_start();
}

void setupWifi()
{
    DEBUG_PRINTLN("Connecting to " + String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startMillis > 10000)
        {
            DEBUG_PRINTLN("Taken too long to connect to WiFi. Going to sleep");
            goToSleep();
        }
    }
    rssiLevel = map(WiFi.RSSI(), -98, -50, 0, 10);
    DEBUG_PRINTLN("WiFi connected, RSSI: " + String(rssiLevel));
}

void sendToServer(String url)
{
    http.begin(HOST, PORT, url);
    int httpCode = http.GET();
    if (httpCode)
    {
        if (httpCode == 200)
        {
            String payload = http.getString();
            DEBUG_PRINTLN(payload);
        }
    }
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
    // DEBUG_PRINTLN("Temp: " + String(tempOld));
    // DEBUG_PRINTLN("Bar: " + String(barOld));
    // DEBUG_PRINTLN("Lux: " + String(luxOld));
    if ((abs(temp - tempOld) > 0.5) || (abs(bar - barOld) > 1) || (abs(lux - luxOld) > 5))
    {
        tempOld = temp;
        barOld = bar;
        luxOld = lux;
        // DEBUG_PRINTLN("Temp new: " + String(tempOld));
        // DEBUG_PRINTLN("Bar new: " + String(barOld));
        // DEBUG_PRINTLN("Lux new: " + String(luxOld));
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
    init();
    getMeasurements();
    transmitData();
    goToSleep();
}

void loop()
{
}