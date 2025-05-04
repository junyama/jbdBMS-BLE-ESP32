/*
*******************************************************************************
* Copyright (c) 2021 by M5Stack
*                  Equipped with M5Core2 sample source code
* Visit for more information: https://docs.m5stack.com/en/core/core2
*
* Describe: WIFI Multi.
* Date: 2021/7/29
*******************************************************************************
*  Connect to the best AP based on a given wifi list
*/

#include <M5Core2.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPDateTime.h>
#include <Ambient.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

// #include "MyBLE.hpp"
#include "MyBLE2.hpp"

#include "MyDebug.hpp"
#include "MySdCard.hpp"
// #include "MyAmbient.hpp"
#include "MyAmbient2.hpp"
// #include "MyMqtt.hpp"
#include "MyMqtt2.hpp"

// #include "PowerSaving.hpp"
#include "PowerSaving2.hpp"
// #include "MyLcd.hpp"
#include "MyLcd2.hpp"

#include "VoltMater.hpp"

using namespace MyLOG;

// #include <JbdBms.h>
// #include <LittleFS.h>

// #define LittleFS SPIFFS
#define CONFIG_FILE "config.json"

// #define WIFI_LED 32
// #define BLE_LED 33 // this constant is not used bu main but used by MyCallback

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
// #define TIME_TO_SLEEP 900      /* Time ESP32 will go to sleep (in seconds) */

// #define NUMBER_OF_DEVICES 2

static const String TAG = "main";

// StaticJsonDocument<1024> configJson;
JsonDocument configJson;
DeserializationError error = deserializeJson(configJson, "{\"numberOfTemperature\": 2, \"sleepVoltageMv\": 12999, \"wakeUpVoltageMv\": 13899, \"deepSleepVoltageMv\": 11699, \"deepSleepTimeSec\": 900, \"wifi\": [{\"ssid\": \"Jun-Home-AP\", \"pass\": \"xxxxx\"}, {\"ssid\": \"Jun-FS020W\", \"pass\": \"xxxxx\"}], \"poiURL\": \"http://junichi2.ddns.net/\", \"ambient\": {\"channelId\": 50366, \"writeKey\": \"ccb476294fe16acd\", \"ambientSendIntervalBaseMs\": 60000}}");

// Wi-Fi client
WiFiClient wifiClient;

// WiFiMulti
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 20000;

// Web server
AsyncWebServer server(80);

// int batteryTemp1, batteryTemp2, batteryChargePercentage, batteryCurrent, batteryVoltage, cellDiffVoltage, batteryCycleCount, mosFet, cellBalance;
bool cellBalanceList[4];
bool chargeStatus, dischargeStatus;

// Ambient service
// unsigned int channelId = 1234;
// String writeKey = "xxxxxxxxxxxxxx";
// unsigned long ambientlLastSent = 0;
// unsigned int ambientSendIntervalBaseMs = 60 * 1000; // milli sec
// unsigned int ambientSendIntervalMs = ambientSendIntervalBaseMs;
// Ambient ambientClient;
MyAmbient2 ambientClient2;

// sleep control
unsigned int numberOfTemperature = 2; // numbe of temperature sensor
float sleepVoltage = 13.399 * 1000;   // mV
unsigned int sleepVoltageMv = 13199;  // mV
unsigned int wakeUpVoltageMv = 13399;
unsigned int deepSleepVoltageMv = 13199; // mV
unsigned int deepSleepTimeSec = 900;     // seconds
unsigned int rebootCount = 0;
unsigned int rebootLimit = 10;

// BLE
// MyBLE2 myBLE;
// MyBLE2 *myBleArr = new MyBLE2[NUMBER_OF_DEVICES];
MyBLE2 myBleArr[NUMBER_OF_DEVICES];

// Volt Mater
VoltMater voltMater;

// MQTT
MyMqtt2 mqttClient2;
// MyMqtt2 *mqttClientArr = new MyMqtt2[2];

// PubSubClient mqttClient(wifiClient);
// MyMqtt2 mqttClient2(&mqttClient, wifiClient, &myBLE, &voltMater);

// LCD
MyLcd2 myLcd;

// Power saving
PowerSaving2 powerSaving;

// local functions definitions
void setupDateTime()
{
  // setup this after wifi connected
  // you can use custom timeZone,server and timeout
  // DateTime.setTimeZone("CST-8");
  DateTime.setTimeZone("JST-9");
  DateTime.setServer("ntp.nict.jp");
  // DateTime.begin(15 * 1000);
  // from
  /** changed from 0.2.x **/
  DateTime.begin(15 * 1000 /* timeout param */);
  if (DateTime.isTimeValid())
  {
    LOGD(TAG, "DateTime setup done");
    LOGLCD(TAG, "DateTime setup done");
  }
  else
    LOGD(TAG, "Failed to get time from server.");
}

void wifiScann()
{
  int n = WiFi.scanNetworks();
  LOGD(TAG, "scan done");
  if (n == 0)
  {
    LOGD(TAG, "no networks found");
  }
  else
  {
    Serial.print(n);
    LOGD(TAG, " networks found");
    LOGLCD(TAG, " networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      LOGD(TAG, (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
}

int wifiConnect()
{
  LOGD(TAG, "Connecting Wifi...");
  M5.Lcd.println("Connecting Wifi..."); // Serial port format output string.

  // if the connection to the stongest hotstop is lost, it will connect to the next network on the list
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED)
  {
    String logText = "WiFi connected to " + WiFi.SSID();
    logText += "(" + String(WiFi.RSSI()) + ")";
    LOGD(TAG, logText);
    // logText = "IP: ";
    // logText += String(WiFi.localIP());
    // LOGD(TAG, logText);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    // digitalWrite(WIFI_LED, HIGH);
    // LOGD(TAG, "WIFI_LED ON");

    // M5.Lcd.setCursor(0, 20);
    M5.Lcd.print("WiFi connected to\nSSID:");
    M5.Lcd.println(WiFi.SSID()); // Output Network name.
    M5.Lcd.print("RSSI: ");
    M5.Lcd.println(WiFi.RSSI()); // Output signal strength.
    M5.Lcd.print("IP address: ");
    M5.Lcd.println(WiFi.localIP()); // Output IP Address.

    return 0;
  }
  else
  {
    LOGD(TAG, "WiFi not connected!");
    return -1;
  }
}

String getValues(int deviceId)
{
  String jsonStr = "";
  jsonStr.reserve(300);
  jsonStr += "{\"batteryTemp1\": ";
  jsonStr += String(myBleArr[deviceId].packBasicInfo.Temp1);
  jsonStr += ", \"batteryTemp2\": ";
  if (numberOfTemperature == 2)
    jsonStr += String(myBleArr[deviceId].packBasicInfo.Temp2);
  // else
  // jsonStr += String(myBleArr[deviceId]packBasicInfo.Temp1);
  jsonStr += ", \"batteryChargePercentage\": ";
  jsonStr += String(myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
  jsonStr += ", \"batteryCurrent\": ";
  jsonStr += String(myBleArr[deviceId].packBasicInfo.Amps / 10);
  // jsonStr += ", \"batteryCycleCount\": ";
  // jsonStr += String(batteryCycleCount);
  jsonStr += ", \"batteryVoltage\": ";
  jsonStr += String(myBleArr[deviceId].packBasicInfo.Volts / 10);
  jsonStr += ", \"mosfetStatus\": {\"chargeStatus\": ";
  chargeStatus = myBleArr[deviceId].packBasicInfo.MosfetStatus & 1;
  jsonStr += String(chargeStatus);
  jsonStr += ", \"dischargeStatus\": ";
  // dischargeStatus = myBleArr[deviceId].packBasicInfo.MosfetStatus & 1 << 1;
  dischargeStatus = (myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1;
  jsonStr += String(dischargeStatus);
  jsonStr += "}, \"batteryList\": [";
  jsonStr += String(myBleArr[deviceId].packCellInfo.CellVolt[0]);
  for (int i = 1; i < myBleArr[deviceId].packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(myBleArr[deviceId].packCellInfo.CellVolt[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"batteryDiff\": ";
  jsonStr += String(myBleArr[deviceId].packCellInfo.CellDiff);
  for (int i = 0; i < myBleArr[deviceId].packCellInfo.NumOfCells; i++)
  {
    cellBalanceList[i] = myBleArr[deviceId].packBasicInfo.BalanceCodeLow & 1 << i;
  }
  jsonStr += ", \"cellBalanceList\": [";
  jsonStr += String(cellBalanceList[0]);
  for (int i = 1; i < myBleArr[deviceId].packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellBalanceList[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"cellMedian\": ";
  jsonStr += String(myBleArr[deviceId].packCellInfo.CellMedian);
  jsonStr += ", \"BLEConnected\": ";
  jsonStr += String(myBleArr[deviceId].myClientCallback->BLE_client_connected);
  // jsonStr += String(BLE_client_connected);
  jsonStr += "}";
  return jsonStr;
}

String disconnectBLE(int deviceId)
{
  myBleArr[deviceId].ctrlCommand = 2;
  return "OK";
}

String requestDeviceName(int deviceId)
{
  myBleArr[deviceId].ctrlCommand = 3;
  return "OK";
}

String getDeviceName(int deviceId)
{
  return myBleArr[deviceId].deviceNameStr;
}

/*
void getDeviceNameLoop(MyBLE2 *myBLE)
{
  String deviceNameStr;
  // myBLE.deviceNameStr = "";
  // while (true)
  for (int i = 0; i < 20; i++)
  {
    myBLE->bleRequestData();
    if (myBLE->newPacketReceived == true)
    {
      deviceNameStr = myBLE->deviceNameStr;
      if (deviceNameStr)
      {
        LOGD(TAG, "deviceNameStr: " + deviceNameStr);
        M5.Lcd.println(deviceNameStr);
        return;
      }
      LOGD(TAG, "deviceNameStr: null");
      delay(500);
    }
  }
}
*/

String reset()
{
  LOGD(TAG, "going to reset in 5 sec");
  delay(5000);
  // ESP.restart();
  M5.shutdown(10);
  return "OK";
}

void loadConfig()
{
  String fileName = "/";
  fileName += CONFIG_FILE;
  String textStr = "";
  MySdCard::readFile(SD, fileName.c_str(), textStr);

  LOGD(TAG, "configJsonText: " + textStr);
  DeserializationError error = deserializeJson(configJson, textStr.c_str());
  if (error)
  {
    LOGD(TAG, "Deserialization error.");
    return;
  }
  /*
  int numberOfTemperature_ = configJson["numberOfTemperature"];
  if (numberOfTemperature_)
  {
    numberOfTemperature = numberOfTemperature_;
    myBLE.numberOfTemperature = numberOfTemperature_;
  }
  */
  int sleepVoltageMv_ = configJson["sleepVoltageMv"];
  if (sleepVoltageMv_)
  {
    sleepVoltageMv = sleepVoltageMv_;
    LOGD(TAG, "sleepVoltageMv: " + String(sleepVoltageMv));
  }
  int wakeUpVoltageMv_ = configJson["wakeUpVoltageMv"];
  if (wakeUpVoltageMv_)
    wakeUpVoltageMv = wakeUpVoltageMv_;
  int deepSleepVoltageMv_ = configJson["deepSleepVoltageMv"];
  if (deepSleepVoltageMv_)
    deepSleepVoltageMv = deepSleepVoltageMv_;
  int deepSleepTimeSec_ = configJson["deepSleepTimeSec"];
  if (deepSleepTimeSec_)
    deepSleepTimeSec = deepSleepTimeSec_;
  int rebootCount_ = configJson["rebootCount"];
  if (rebootCount_)
    rebootCount = rebootCount_;
  int rebootLimit_ = configJson["rebootLimit"];
  if (rebootLimit_)
    rebootLimit = rebootLimit_;
}

void saveConfig()
{
  String fileName = "/";
  fileName += CONFIG_FILE;
  String jsonStr;
  serializeJsonPretty(configJson, jsonStr);
  LOGD(TAG, "writing configuration file: " + fileName);
  MySdCard::writeFile(SD, fileName.c_str(), jsonStr.c_str());
}

void updatePOI()
{
  JsonDocument poiIndexJson;
  HTTPClient http;
  const char *poiURL_ = configJson["poiURL"];
  String poiURL = poiURL_;
  http.begin(poiURL + "poi/index.json");
  int httpResponseCode = http.GET();
  if (httpResponseCode == 200)
  {
    char buff[128];
    sprintf(buff, "HTTP Response code: %d", httpResponseCode);
    LOGD(TAG, buff);
    String indexJsonStr = http.getString();
    Serial.println(indexJsonStr);
    DeserializationError error = deserializeJson(poiIndexJson, indexJsonStr);
    if (error)
    {
      LOGD(TAG, "deserializeJson() failed");
      LOGD(TAG, "error description: " + String(error.f_str()));
      return;
    }
    else
    {
      M5.Lcd.println("Updating POI...");

      MySdCard::listDir(SD, "/PersonalPOI", 0);
      MySdCard::removeDirR(SD, "/PersonalPOI");
      MySdCard::createDir(SD, "/PersonalPOI");
      MySdCard::writeFile(SD, "/PersonalPOI/index.json", indexJsonStr.c_str());
      JsonArray poiIndexArray = poiIndexJson.as<JsonArray>();
      for (JsonVariant v : poiIndexArray)
      {
        String POIFileName = v.as<String>();
        LOGD(TAG, "POI file name: " + POIFileName);
        http.begin(poiURL + "poi/" + POIFileName);
        httpResponseCode = http.GET();
        if (httpResponseCode == 200)
        {
          String gpxStr = http.getString();
          Serial.println(gpxStr);
          // writeFile("/poi/" + POIFileName, gpxStr);
          String path = "/PersonalPOI/" + POIFileName;
          MySdCard::writeFile(SD, path.c_str(), gpxStr.c_str());
        }
        else
        {
          LOGD(TAG, "GET " + POIFileName + " failed, HTTP Response code: " + String(httpResponseCode));
        }
      }
      M5.Lcd.println("POI update done!");
      delay(5000);
    }
  }
  else
  {
    LOGD(TAG, "GET index.json failed, HTTP Response code: " + String(httpResponseCode));
  }
  // Free resources
  http.end();
  // SD.end();
  return;
}

void sleep(int sec)
{
  sec = 5;
  WiFi.disconnect(true);

  M5.Axp.SetLed(0);
  M5.Axp.SetLcdVoltage(0);
  myBleArr[0].disconnectFromServer();
  M5.Axp.DeepSleep(SLEEP_SEC(sec));
}

void myDeepSleep(int sec) // link error
{
  WiFi.disconnect(true);

  M5.Axp.SetLed(0);
  M5.Axp.SetLcdVoltage(0);
  M5.Axp.DeepSleep(SLEEP_SEC(sec));
}

/*
void showMainBatteryVoltage(ADS1115 voltMater)
{
  int16_t adc_raw = voltMater.getSingleConversion();
  float voltage = adc_raw * resolution * calibration_factor;
  char str[128];
  sprintf(str, "Cal ADC:%.0f", adc_raw * calibration_factor);
  LOGD(TAG, str);
  sprintf(str, "Cal Voltage:%.2f mV", voltage);
  LOGD(TAG, str);
  sprintf(str, "Raw ADC:%d\n", adc_raw);
  LOGD(TAG, str);
}
*/

void setup()
{
  M5.begin(); // Init M5Core2.
  Serial.begin(9600);

  MySdCard::deleteFile(SD, "/log.txt");

  // MyLcd::setup();
  myLcd.setup();

  // LITTLEFS
  LOGD(TAG, "mounting SPIFFS");
  if (!SPIFFS.begin(true))
  {
    LOGD(TAG, "SPIFFS mount failed");
    return;
  }
  else
  {
    LOGD(TAG, "SPIFFS mount done");
  }

  // SD card setup
  MySdCard::setup();

  // load config.json from SD
  loadConfig();
  if (rebootCount > rebootLimit)
  {
    rebootCount = 0;
    configJson["rebootCount"] = 0;
    saveConfig();
    String logStr = "going to deep sleep because exceeding reboot limit (" + String(rebootLimit) + "). Wake up in " + String(deepSleepTimeSec) + "sec";
    LOGD(TAG, logStr);
    myLcd.println(logStr);
    mqttClient2.publish("stat/" + configJson["mqtt"]["topic"] + "STATE", logStr);
    delay(3000);
    // PowerSaving::enable();
    powerSaving.enable();
    // sleep(deepSleepTimeSec);
    M5.shutdown(deepSleepTimeSec);
  }
  else
  {
    rebootCount++;
    LOGD(TAG, "rebootCount incremented: " + String(rebootCount));
    configJson["rebootCount"] = rebootCount;
    saveConfig();
  }

  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("JunBMS");

  // Add list of wifi networks
  for (int i = 0; i < configJson["wifi"].size(); i++)
  {
    wifiMulti.addAP(configJson["wifi"][i]["ssid"], configJson["wifi"][i]["pass"]);
  }
  //
  LOGD(TAG, "going to scann WiFi");
  wifiScann();

  LOGD(TAG, "going to connect WiFi");
  if (wifiConnect() != 0)
  {
    LOGD(TAG, "failed to connect WiFi and exiting");
    exit(-1);
  }
  LOGD(TAG, "WiFi setup done");

  // setup DateTime
  LOGD(TAG, "Going to setup date");
  setupDateTime();

  // update POI in SD card
  LOGD(TAG, "Going to update POI");
  updatePOI(); // TBD

  // setup webAPIs
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html"); });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/favicon.ico"); });
  server.on("/getValues", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "appicatlion/json", getValues(0).c_str()); });
  server.on("/disconnectBLE", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", disconnectBLE(0).c_str()); });
  server.on("/requestDeviceName", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", requestDeviceName(0).c_str()); });
  server.on("/getDeviceName", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getDeviceName(0).c_str()); });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", reset().c_str()); });
  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/mosfetCtrl", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                         {
    //LOGD(TAG, "/mosfetCtrl called");
    JsonObject jsonObj = json.as<JsonObject>();
    String jsonStr;
    serializeJsonPretty(jsonObj, jsonStr);
    LOGD(TAG, "posted json: " + jsonStr);
    myBleArr[0].ctrlCommand = 1;
    myBleArr[0].commandParam = (byte)jsonObj["chargeStatus"] + (byte)jsonObj["dischargeStatus"] * 2;
    //request->send(200, "application/json", "{\"message\": \"OK\"}");
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    root["dischargeStatus"] = dischargeStatus;
    root["chargeStatus"] = chargeStatus;
    response->setLength();
    request->send(response); });
  server.addHandler(handler);
  server.begin();

  //Ambient setup
  ambientClient2.begin(configJson, &wifiClient);

  // MQTT setup
  M5.Lcd.println("MQTT setting up!");
  mqttClient2.setup(&wifiClient, &myBleArr[0], &voltMater, configJson);

  // Home aAssistant discoverry
  M5.Lcd.println("Publishing HA discvery.");
  mqttClient2.publishHaDiscovery();

  M5.Lcd.println("MQTT setup done!");

  // Volt meter setup
  voltMater.setup(configJson);
  LOGD(TAG, "Volt Mater setup done");
  M5.Lcd.println("Volt Mater setup done!");

  //  setup BLE
  LOGD(TAG, "going to setup BLE");
  M5.Lcd.println("going to setup BLE");

  // myBLE.configJson = &configJson;
  // myBLE.packBasicInfo.Volts = 15000;
  // myBLE.bleStartup();
  //
  LOGD(TAG, "going to setup BLE Array");
  for (int i = 0; i < NUMBER_OF_DEVICES; i++)
  {
    Serial.printf("\n\nmyBleArr[%d] ---------------------------------------------------------\n", i);
    new (myBleArr + i) MyBLE2();
    myBleArr[i].deviceConfig = configJson["devices"][i];

    myBleArr[i].configJson = &configJson;
    myBleArr[i].numberOfTemperature = configJson["numberOfTemperature"];
    myBleArr[i].packBasicInfo.Volts = 15000;
    LOGD(TAG, "going to call bleStartup()");
    myBleArr[i].bleStartup();
    LOGD(TAG, "going to call getDeviceNameLoop(&myBleArr[i])");
    myBleArr[i].getDeviceNameLoop();
  }
  //
  // LOGD(TAG, "getting device name....");
  // getDeviceNameLoop(&myBLE);
  // myBLE.getDeviceNameLoop();

  LOGD(TAG, "BLE setup done");
  M5.Lcd.println("BLE setup done!");

  // esp_sleep_enable_timer_wakeup(deepSleepTimeSec * uS_TO_S_FACTOR);
  // LOGD(TAG, "Setup ESP32 to sleep for " + String(deepSleepTimeSec) + " Seconds");

  // Button setup
  // PowerSaving::setup();
  powerSaving.setup();

  M5.Lcd.println("ALL setup done!");
  M5.Lcd.println("enabling power save.");
  delay(2000);
  // PowerSaving::enable();
  powerSaving.enable();
}
/////////////////////////////
void loop()
{
  powerSaving.loop();
  for (int deviceId = 0; deviceId < NUMBER_OF_DEVICES; deviceId++)
  {
    myBleArr[deviceId].bleRequestData();
    if (myBleArr[deviceId].newPacketReceived == true)
    {
      LOGD(TAG, "newPacketReceived == true");
      DISABLE_LOGD = true;
      myBleArr[deviceId].printBasicInfo();
      DISABLE_LOGD = false;
      LOGD(TAG, "Pack Voltage: " + String(myBleArr[deviceId].packBasicInfo.Volts));
      DISABLE_LOGD = true;
      LOGD(TAG, "BalanceCodeLow: " + String(myBleArr[deviceId].packBasicInfo.BalanceCodeLow));
      LOGD(TAG, "MosfetStatus: " + String(myBleArr[deviceId].packBasicInfo.MosfetStatus));
      LOGD(TAG, "CellAvg: " + String(myBleArr[deviceId].packCellInfo.CellAvg));
      LOGD(TAG, "CellMedian: " + String(myBleArr[deviceId].packCellInfo.CellMedian));
      myBleArr[deviceId].printCellInfo();
      DISABLE_LOGD = false;

      mqttClient2.publishJson("stat/" + myBleArr[deviceId].deviceTopic + "STATE", mqttClient2.getState2(), true);
      mqttClient2.publishJson("stat/" + myBleArr[deviceId].deviceTopic + "STATE", voltMater.getVoltage(), true);
      voltMater.lastMeasurment = millis();

      myLcd.showBatteryInfo(myBleArr[deviceId].packBasicInfo.Volts / 1000.0f, myBleArr[deviceId].packBasicInfo.Amps / 1000.0f, myBleArr[deviceId].packCellInfo.CellDiff / 1.0f, myBleArr[deviceId].packBasicInfo.Temp1 / 10.0f, voltMater.calVoltage, myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
    }
    if (myBleArr[deviceId].packBasicInfo.Volts <= sleepVoltageMv && WiFi.isConnected())
    {
      String logStr = "disconnecting WiFi, batteryVoltage: " + String(myBleArr[deviceId].packBasicInfo.Volts) + " <= " + String(sleepVoltageMv);
      LOGD(TAG, logStr);
      mqttClient2.publish("stat/" + mqttClient2.hostTopic + "STATE", logStr);
      delay(2000);
      WiFi.disconnect(true);
      delay(3000);
      powerSaving.enable();
      ambientClient2.setLongInterval();
    }
    else
    {
      if (WiFi.isConnected())
      {
        /*
        if (!mqttClient2.connected())
        {
          if (mqttClient2.reConnect())
          {
            mqttClient2.publish("stat/" + mqttClient2.topic + "STATE", "resetting system because of reconnecting MQTT server failed.");
            delay(2000);
            reset();
          }
        }
        */
        mqttClient2.loop();
      }
    }
    if (myBleArr[deviceId].packBasicInfo.Volts > wakeUpVoltageMv && !WiFi.isConnected())
    {
      wifiConnect();
      LOGD(TAG, "woke up and WiFi reconnected, batteryVoltage: " + String(myBleArr[deviceId].packBasicInfo.Volts) + " > " + String(sleepVoltageMv));
      // ambientSendIntervalMs = ambientSendIntervalBaseMs;
      ambientClient2.resetInterval();
    }
    //
    // if ((millis() - ambientlLastSent) >= ambientSendIntervalMs)
    if (ambientClient2.timeout(millis()))
    {
      // LOGD(TAG, "millis() - ambientlLastSent: " + String(millis()) + " - " + String(ambientlLastSent) + " >= ambientSendIntervalMs: " + String(ambientSendIntervalMs));
      if (!WiFi.isConnected())
      {
        wifiConnect();
      }
      float values[7];
      values[0] = myBleArr[deviceId].packBasicInfo.Volts / 1000.0f;
      values[1] = myBleArr[deviceId].packBasicInfo.Amps / 1000.0f;
      values[2] = myBleArr[deviceId].packCellInfo.CellDiff / 1.0f;
      values[3] = myBleArr[deviceId].packBasicInfo.Temp1 / 10.0f;
      if (numberOfTemperature == 2)
        values[3] = (myBleArr[deviceId].packBasicInfo.Temp1 + myBleArr[deviceId].packBasicInfo.Temp2) / 2 / 10.0f;
      values[4] = myBleArr[deviceId].packBasicInfo.CapacityRemainPercent;
      values[5] = M5.Axp.GetBatVoltage();
      values[6] = M5.Axp.GetBatCurrent();
      ambientClient2.set(values);
      ambientClient2.send();
      ambientClient2.ambientlLastSent = millis();

      mqttClient2.publishJson("stat/" + mqttClient2.hostTopic + "STATE", voltMater.getVoltage(), true);
      voltMater.lastMeasurment = millis();

      if (myBleArr[deviceId].packBasicInfo.Volts <= deepSleepVoltageMv)
      {
        String logStr = "Going to deep sleep now and wake up in " + String(deepSleepTimeSec) + " seconds";
        LOGD(TAG, logStr);
        myLcd.println(logStr);
        mqttClient2.publish("stat/" + mqttClient2.hostTopic + "STATE", logStr);
        delay(2500);
        // esp_deep_sleep_start(); //link error
        // M5.Axp.DeepSleep(SLEEP_SEC(5)); // link error
        // PowerSaving::enable();
        // sleep(deepSleepTimeSec);
        M5.shutdown(deepSleepTimeSec);
        // myDeepSleep(deepSleepTimeSec); // link error
        // LOGD(TAG, "This will never be printed");
      }
      else
        LOGD(TAG, "PackVoltage: " + String(myBleArr[deviceId].packBasicInfo.Volts) + " > " + String(deepSleepVoltageMv));
    }
    if (voltMater.timeout(millis()))
    {
      mqttClient2.publishJson("stat/" + mqttClient2.hostTopic + "STATE", voltMater.getVoltage(), true);
      voltMater.lastMeasurment = millis();
    }
  }
}