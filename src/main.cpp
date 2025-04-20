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

using namespace MyLOG;

// #include <JbdBms.h>
// #include <LittleFS.h>

// #define LittleFS SPIFFS
#define CONFIG_FILE "config.json"

// #define WIFI_LED 32
// #define BLE_LED 33 // this constant is not used bu main but used by MyCallback

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
// #define TIME_TO_SLEEP 900      /* Time ESP32 will go to sleep (in seconds) */

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
MyBLE2 myBLE(&configJson);

// MQTT
PubSubClient mqttClient(wifiClient);
MyMqtt2 mqttClient2(&mqttClient, &myBLE);

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

String getValues()
{
  String jsonStr = "";
  jsonStr.reserve(300);
  jsonStr += "{\"batteryTemp1\": ";
  jsonStr += String(myBLE.packBasicInfo.Temp1);
  jsonStr += ", \"batteryTemp2\": ";
  if (numberOfTemperature == 2)
    jsonStr += String(myBLE.packBasicInfo.Temp2);
  // else
  // jsonStr += String(myBLEpackBasicInfo.Temp1);
  jsonStr += ", \"batteryChargePercentage\": ";
  jsonStr += String(myBLE.packBasicInfo.CapacityRemainPercent);
  jsonStr += ", \"batteryCurrent\": ";
  jsonStr += String(myBLE.packBasicInfo.Amps / 10);
  // jsonStr += ", \"batteryCycleCount\": ";
  // jsonStr += String(batteryCycleCount);
  jsonStr += ", \"batteryVoltage\": ";
  jsonStr += String(myBLE.packBasicInfo.Volts / 10);
  jsonStr += ", \"mosfetStatus\": {\"chargeStatus\": ";
  chargeStatus = myBLE.packBasicInfo.MosfetStatus & 1;
  jsonStr += String(chargeStatus);
  jsonStr += ", \"dischargeStatus\": ";
  // dischargeStatus = myBLE.packBasicInfo.MosfetStatus & 1 << 1;
  dischargeStatus = (myBLE.packBasicInfo.MosfetStatus & 2) >> 1;
  jsonStr += String(dischargeStatus);
  jsonStr += "}, \"batteryList\": [";
  jsonStr += String(myBLE.packCellInfo.CellVolt[0]);
  for (int i = 1; i < myBLE.packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(myBLE.packCellInfo.CellVolt[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"batteryDiff\": ";
  jsonStr += String(myBLE.packCellInfo.CellDiff);
  for (int i = 0; i < myBLE.packCellInfo.NumOfCells; i++)
  {
    cellBalanceList[i] = myBLE.packBasicInfo.BalanceCodeLow & 1 << i;
  }
  jsonStr += ", \"cellBalanceList\": [";
  jsonStr += String(cellBalanceList[0]);
  for (int i = 1; i < myBLE.packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellBalanceList[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"cellMedian\": ";
  jsonStr += String(myBLE.packCellInfo.CellMedian);
  jsonStr += ", \"BLEConnected\": ";
  jsonStr += String(myBLE.myClientCallback->BLE_client_connected);
  // jsonStr += String(BLE_client_connected);
  jsonStr += "}";
  return jsonStr;
}

String disconnectBLE()
{
  myBLE.ctrlCommand = 2;
  return "OK";
}

String requestDeviceName()
{
  myBLE.ctrlCommand = 3;
  return "OK";
}

String getDeviceName()
{
  return myBLE.deviceNameStr;
}

void getDeviceNameLoop()
{
  String deviceNameStr;
  // myBLE.deviceNameStr = "";
  // while (true)
  for (int i = 0; i < 20; i++)
  {
    myBLE.bleRequestData();
    if (myBLE.newPacketReceived == true)
    {
      deviceNameStr = myBLE.deviceNameStr;
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
  /*
  if (!SD.begin(5))
  {
    LOGD(TAG, "SD Card Mount Failed");
    return;
  }
  //
  LOGD(TAG, "SD Card initalized");
  String fileName = "/";
  fileName += CONFIG_FILE;
  LOGD(TAG, "opeing file from SD Card in read mode");
  File myFile = SD.open(fileName, FILE_READ); // Open the file "/config.json" in read mode.
  if (!myFile)
  {
    LOGD(TAG, "error opening /config.json"); // If the file is not open.
    return;
  }
  String textStr = "";
  while (myFile.available())
  {
    textStr = textStr + myFile.readString();
  }
  myFile.close();
  */
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
  int numberOfTemperature_ = configJson["numberOfTemperature"];
  if (numberOfTemperature_)
  {
    numberOfTemperature = numberOfTemperature_;
    myBLE.numberOfTemperature = numberOfTemperature_;
  }

  /*
  int channelId_ = configJson["ambient"]["channelId"];
  if (channelId_)
    channelId = channelId_;
  const char *writeKey_ = configJson["ambient"]["writeKey"];
  if (*writeKey_)
  {
    writeKey = writeKey_;
    LOGD(TAG, "writeKey: " + writeKey);
  }
  int ambientSendIntervalBaseMs_ = configJson["ambient"]["ambientSendIntervalBaseMs"];
  if (ambientSendIntervalBaseMs_)
  {
    ambientSendIntervalBaseMs = ambientSendIntervalBaseMs_;
    ambientSendIntervalMs = ambientSendIntervalBaseMs;
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
  myBLE.disconnectFromServer();
  M5.Axp.DeepSleep(SLEEP_SEC(sec));
}

void myDeepSleep(int sec) // link error
{
  WiFi.disconnect(true);

  M5.Axp.SetLed(0);
  M5.Axp.SetLcdVoltage(0);
  M5.Axp.DeepSleep(SLEEP_SEC(sec));
}

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
    mqttClient2.publish("stat/" + mqttClient2.topic + "STATE", logStr);
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

  /* static IP address setup
  const IPAddress local_IP(192, 168, 0, 45);
  const IPAddress gateway(192, 168, 0, 1);
  const IPAddress DNS(192, 168, 0, 1);
  const IPAddress subnet(255, 255, 255, 0);
  if (!WiFi.config(local_IP, gateway, subnet, DNS))
  {
    LOGD(TAG, "Failed to configure!");
  }
  */

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

  /*
  server.on("/justgage/raphael.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/raphael.min.js"); });

  server.on("/justgage/justgage.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/justgage.js"); });

  server.on("/log.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/log.txt"); });

  server.on("/poi/index.json", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SD, "/PersonalPOI/index.json"); });

  */
  server.on("/getValues", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "appicatlion/json", getValues().c_str()); });

  server.on("/disconnectBLE", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", disconnectBLE().c_str()); });

  server.on("/requestDeviceName", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", requestDeviceName().c_str()); });

  server.on("/getDeviceName", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getDeviceName().c_str()); });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", reset().c_str()); });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/mosfetCtrl", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                         {
    //LOGD(TAG, "/mosfetCtrl called");
    JsonObject jsonObj = json.as<JsonObject>();
    String jsonStr;
    serializeJsonPretty(jsonObj, jsonStr);
    LOGD(TAG, "posted json: " + jsonStr);
    myBLE.ctrlCommand = 1;
    myBLE.commandParam = (byte)jsonObj["chargeStatus"] + (byte)jsonObj["dischargeStatus"] * 2;

    //request->send(200, "application/json", "{\"message\": \"OK\"}");
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    root["dischargeStatus"] = dischargeStatus;
    root["chargeStatus"] = chargeStatus;
    response->setLength();
    request->send(response); });

  server.addHandler(handler);

  server.begin();

  // init ambient channelID and key
  // ambientClient.begin(channelId, writeKey.c_str(), &wifiClient);
  // MyAmbient::setup(&ambientClient, configJson, &wifiClient);
  // MyAmbient2 ambientClient2_(configJson, &wifiClient);
  // ambientClient2 = ambientClient2_; //not working
  ambientClient2.begin(configJson, &wifiClient);

  // initalize pack volt not to disconnect WiFi
  myBLE.packBasicInfo.Volts = 15000;
  // ambientlLastSent = millis() + 100000;
  // ambientlLastSent = 0;
  // LOGD(TAG, "ambientlLastSent initial value: " + String(ambientlLastSent));
  // LOGD(TAG, "ambient setup done");

  // MQTT setup
  /*
  String mqttServerConf = configJson["MQTT"]["server"];
  if (mqttServerConf != "null")
    mqtt_server = mqttServerConf;
  LOGD(TAG, "MQTT Server: " + mqtt_server);
  int mqqtPortConf = configJson["MQTT"]["port"];
  if (mqqtPortConf)
    mqtt_port = mqqtPortConf;
  LOGD(TAG, "MQTT Server port: " + mqtt_port);
  String mqttTopicConf = configJson["MQTT"]["topic"];
  if (mqttTopicConf != "null")
    mqtt_topic = mqttTopicConf;
  LOGD(TAG, "MQTT Topic: " + mqtt_topic);
  // MyMqtt::setup(&mqttClient, mqtt_server, mqtt_port, mqtt_topic);
  */

  mqttClient2.setup(configJson);

  // Home aAssistant discoverry
  mqttClient2.publishHaDiscovery();

  M5.Lcd.println("MQTT setup done!");

  //  setup BLE
  LOGD(TAG, "going to setup BLE");
  M5.Lcd.println("going to setup BLE");
  myBLE.bleStartup();

  LOGD(TAG, "getting device name....");
  getDeviceNameLoop();

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

void loop()
{
  // PowerSaving::loop();
  powerSaving.loop();

  myBLE.bleRequestData();
  if (myBLE.newPacketReceived == true)
  {
    LOGD(TAG, "newPacketReceived == true");
    DISABLE_LOGD = true;
    myBLE.printBasicInfo();
    DISABLE_LOGD = false;
    LOGD(TAG, "Pack Voltage: " + String(myBLE.packBasicInfo.Volts));
    DISABLE_LOGD = true;
    LOGD(TAG, "BalanceCodeLow: " + String(myBLE.packBasicInfo.BalanceCodeLow));
    LOGD(TAG, "MosfetStatus: " + String(myBLE.packBasicInfo.MosfetStatus));
    LOGD(TAG, "CellAvg: " + String(myBLE.packCellInfo.CellAvg));
    LOGD(TAG, "CellMedian: " + String(myBLE.packCellInfo.CellMedian));
    myBLE.printCellInfo();
    DISABLE_LOGD = false;

    mqttClient2.publishJson("stat/" + mqttClient2.topic + "STATE", mqttClient2.getState2(), true);

    myLcd.showBatteryInfo(myBLE.packBasicInfo.Volts / 1000.0f, myBLE.packBasicInfo.Amps / 1000.0f, myBLE.packCellInfo.CellDiff / 1.0f, myBLE.packBasicInfo.Temp1 / 10.0f, myBLE.packBasicInfo.Temp2 / 10.0f, myBLE.packBasicInfo.CapacityRemainPercent);
  }
  if (myBLE.packBasicInfo.Volts <= sleepVoltageMv && WiFi.isConnected())
  {
    String logStr = "disconnecting WiFi, batteryVoltage: " + String(myBLE.packBasicInfo.Volts) + " <= " + String(sleepVoltageMv);
    LOGD(TAG, logStr);
    // LOGLCD(TAG, logStr);
    mqttClient2.publish("stat/" + mqttClient2.topic + "STATE", logStr);
    delay(2000);
    WiFi.disconnect(true);
    delay(3000);
    // PowerSaving::enable();
    powerSaving.enable();

    // digitalWrite(WIFI_LED, LOW);
    // ambientSendIntervalMs = ambientSendIntervalBaseMs * 10;
    // MyAmbient::setLongInterval();
    ambientClient2.setLongInterval();
  }
  else
  {
    if (WiFi.isConnected())
    {
      /*
      if (!MyMqtt::connected())
      {
        if (MyMqtt::reConnect())
          reset();
      }
      MyMqtt::loop();
      */

      //
      if (!mqttClient2.connected())
      {
        if (mqttClient2.reConnect())
        {
          mqttClient2.publish("stat/" + mqttClient2.topic + "STATE", "resetting system because of reconnecting MQTT server failed.");
          delay(2000);
          reset();
        }
      }
      mqttClient2.loop();
      //
    }
  }
  if (myBLE.packBasicInfo.Volts > wakeUpVoltageMv && !WiFi.isConnected())
  {
    wifiConnect();
    LOGD(TAG, "woke up and WiFi reconnected, batteryVoltage: " + String(myBLE.packBasicInfo.Volts) + " > " + String(sleepVoltageMv));
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
    /*
    ambientClient.set(1, myBLE.packBasicInfo.Volts / 1000.0f);
    ambientClient.set(2, myBLE.packBasicInfo.Amps / 1000.0f);
    ambientClient.set(3, myBLE.packCellInfo.CellDiff / 1.0f);
    ambientClient.set(4, myBLE.packBasicInfo.Temp1 / 10.0f);
    if (numberOfTemperature == 2)
      ambientClient.set(4, (myBLE.packBasicInfo.Temp1 + myBLE.packBasicInfo.Temp2) / 2 / 10.0f);
    ambientClient.send();
    ambientlLastSent = millis();
    */

    float values[7];
    values[0] = myBLE.packBasicInfo.Volts / 1000.0f;
    values[1] = myBLE.packBasicInfo.Amps / 1000.0f;
    values[2] = myBLE.packCellInfo.CellDiff / 1.0f;
    values[3] = myBLE.packBasicInfo.Temp1 / 10.0f;
    if (numberOfTemperature == 2)
      values[3] = (myBLE.packBasicInfo.Temp1 + myBLE.packBasicInfo.Temp2) / 2 / 10.0f;
    values[4] = myBLE.packBasicInfo.CapacityRemainPercent;
    values[5] = M5.Axp.GetBatVoltage();
    values[6] = M5.Axp.GetBatCurrent();
    ambientClient2.set(values);
    ambientClient2.send();
    ambientClient2.ambientlLastSent = millis();

    // MyLcd::showBatteryInfo(myBLE.packBasicInfo.Volts / 1000.0f, myBLE.packBasicInfo.Amps / 1000.0f, myBLE.packCellInfo.CellDiff / 1.0f, myBLE.packBasicInfo.Temp1 / 10.0f, myBLE.packBasicInfo.Temp2 / 10.0f, myBLE.packBasicInfo.CapacityRemainPercent);
    //myLcd.showBatteryInfo(myBLE.packBasicInfo.Volts / 1000.0f, myBLE.packBasicInfo.Amps / 1000.0f, myBLE.packCellInfo.CellDiff / 1.0f, myBLE.packBasicInfo.Temp1 / 10.0f, myBLE.packBasicInfo.Temp2 / 10.0f, myBLE.packBasicInfo.CapacityRemainPercent);

    //String msgStr = mqttClient2.getState();

    // MQTT publish
    /*
    if (!MyMqtt::connected())
    {
      if (MyMqtt::reConnect())
        reset();
    }
    // mqttClient.loop();
    MyMqtt::publish("stat/" + mqtt_topic + "STATE", msgStr);
    //

    //
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
    // mqttClient.loop();
    //mqttClient2.publish("stat/" + mqttClient2.topic + "STATE", msgStr);

    // String logStr = "ambient sent, channelId: " + String(channelId) + ", message: " + msgStr;
    // LOGD(TAG, logStr);
    //  LOGLCD(TAG, logStr);
    //String logStr = "MQTT publised, topic: stat/" + mqttClient2.topic + "STATE";
    //logStr = logStr + ", message: " + msgStr;
    //LOGD(TAG, logStr);
    // LOGLCD(TAG, logStr);
    //

    if (myBLE.packBasicInfo.Volts <= deepSleepVoltageMv)
    {
      String logStr = "Going to deep sleep now and wake up in " + String(deepSleepTimeSec) + " seconds";
      LOGD(TAG, logStr);
      myLcd.println(logStr);
      mqttClient2.publish("stat/" + mqttClient2.topic + "STATE", logStr);
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
      LOGD(TAG, "PackVoltage: " + String(myBLE.packBasicInfo.Volts) + " > " + String(deepSleepVoltageMv));
  }
}