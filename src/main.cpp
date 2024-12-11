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

#include <WiFi.h>
#include <WiFiMulti.h>

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

#include "MyBLE.hpp"
#include "MyDebug.hpp"
#include "MySdCard.hpp"
#include "MyMqtt.hpp"
#include "PowerSaving.hpp"

using namespace MyLOG;

// #include <JbdBms.h>
// #include <LittleFS.h>

#define LittleFS SPIFFS
#define CONFIG_FILE "config.json"

// #define WIFI_LED 32
// #define BLE_LED 33 // this constant is not used bu main but used by MyCallback

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
// #define TIME_TO_SLEEP 900      /* Time ESP32 will go to sleep (in seconds) */

static const String TAG = "main";

// StaticJsonDocument<1024> configJson;
JsonDocument configJson;
DeserializationError error = deserializeJson(configJson, "{\"numberOfTemperature\": 1, \"sleepVoltageMv\": 12999, \"wakeUpVoltageMv\": 13899, \"deepSleepVoltageMv\": 11699, \"deepSleepTimeSec\": 900, \"wifi\": [{\"ssid\": \"Jun-Home-AP\", \"pass\": \"takehiro\"}, {\"ssid\": \"Jun-FS020W\", \"pass\": \"takehiro\"}], \"poiURL\": \"http://junichi2.ddns.net/\", \"ambient\": {\"channelId\": 50366, \"writeKey\": \"ccb476294fe16acd\", \"ambientSendIntervalBaseMs\": 60000}}");

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
unsigned int channelId = 1234;
String writeKey = "xxxxxxxxxxxxxx";
unsigned long ambientlLastSent = 0;
unsigned int ambientSendIntervalBaseMs = 60 * 1000; // milli sec
unsigned int ambientSendIntervalMs = ambientSendIntervalBaseMs;
Ambient ambient;

// sleep control
// unsigned int numberOfTemperature = 2; // numbe of temperature sensor
float sleepVoltage = 13.399 * 1000;  // mV
unsigned int sleepVoltageMv = 13199; // mV
unsigned int wakeUpVoltageMv = 13399;
unsigned int deepSleepVoltageMv = 13199; // mV
unsigned int deepSleepTimeSec = 900;     // seconds
unsigned int rebootCount = 0;
unsigned int rebootLimit = 5;

// MQTT
PubSubClient mqttClient(wifiClient);
String mqtt_server = "broker.emqx.io";  // default
int mqtt_port = 1883;                   // default
String mqtt_topic = "junichi/M5Core2/"; // default

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
  M5.lcd.print("Connecting Wifi..."); // Serial port format output string.

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

    M5.lcd.setCursor(0, 20);
    M5.lcd.print("WiFi connected\n\nSSID:");
    M5.lcd.println(WiFi.SSID()); // Output Network name.
    M5.lcd.print("RSSI: ");
    M5.lcd.println(WiFi.RSSI()); // Output signal strength.
    M5.lcd.print("IP address: ");
    M5.lcd.println(WiFi.localIP()); // Output IP Address.

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
  jsonStr += String(MyBLE::packBasicInfo.Temp1);
  jsonStr += ", \"batteryTemp2\": ";
  // if (numberOfTemperature == 2)
  jsonStr += String(MyBLE::packBasicInfo.Temp2);
  // else
  // jsonStr += String(MyBLE::packBasicInfo.Temp1);
  jsonStr += ", \"batteryChargePercentage\": ";
  jsonStr += String(MyBLE::packBasicInfo.CapacityRemainPercent);
  jsonStr += ", \"batteryCurrent\": ";
  jsonStr += String(MyBLE::packBasicInfo.Amps / 10);
  // jsonStr += ", \"batteryCycleCount\": ";
  // jsonStr += String(batteryCycleCount);
  jsonStr += ", \"batteryVoltage\": ";
  jsonStr += String(MyBLE::packBasicInfo.Volts / 10);
  jsonStr += ", \"mosfetStatus\": {\"chargeStatus\": ";
  chargeStatus = MyBLE::packBasicInfo.MosfetStatus & 1;
  jsonStr += String(chargeStatus);
  jsonStr += ", \"dischargeStatus\": ";
  // dischargeStatus = MyBLE::packBasicInfo.MosfetStatus & 1 << 1;
  dischargeStatus = (MyBLE::packBasicInfo.MosfetStatus & 2) >> 1;
  jsonStr += String(dischargeStatus);
  jsonStr += "}, \"batteryList\": [";
  jsonStr += String(MyBLE::packCellInfo.CellVolt[0]);
  for (int i = 1; i < MyBLE::packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(MyBLE::packCellInfo.CellVolt[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"batteryDiff\": ";
  jsonStr += String(MyBLE::packCellInfo.CellDiff);
  for (int i = 0; i < MyBLE::packCellInfo.NumOfCells; i++)
  {
    cellBalanceList[i] = MyBLE::packBasicInfo.BalanceCodeLow & 1 << i;
  }
  jsonStr += ", \"cellBalanceList\": [";
  jsonStr += String(cellBalanceList[0]);
  for (int i = 1; i < MyBLE::packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellBalanceList[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"cellMedian\": ";
  jsonStr += String(MyBLE::packCellInfo.CellMedian);
  jsonStr += ", \"BLEConnected\": ";
  jsonStr += String(MyBLE::myClientCallback->BLE_client_connected);
  // jsonStr += String(BLE_client_connected);
  jsonStr += "}";
  return jsonStr;
}

String disconnectBLE()
{
  MyBLE::ctrlCommand = 2;
  return "OK";
}

String requestDeviceName()
{
  MyBLE::ctrlCommand = 3;
  return "OK";
}

String getDeviceName()
{
  return MyBLE::deviceNameStr;
}

String reset()
{
  LOGD(TAG, "going to reset in 5 sec");
  delay(5000);
  ESP.restart();
  return "OK";
}

void loadConfig()
{
  if (!SD.begin(5))
  {
    LOGD(TAG, "SD Card Mount Failed");
    return;
  }
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
  LOGD(TAG, "configJsonText: " + textStr);
  DeserializationError error = deserializeJson(configJson, textStr.c_str());
  if (error)
  {
    LOGD(TAG, "Deserialization error.");
    return;
  }
  int numberOfTemperature = configJson["numberOfTemperature"];
  if (numberOfTemperature)
    MyBLE::numberOfTemperature = numberOfTemperature;
  int channelId_ = configJson["ambient"]["channelId"];
  if (channelId_)
    channelId = channelId_;
  const char *writeKey_ = configJson["ambient"]["writeKey"];
  if (writeKey_)
  {
    writeKey = writeKey_;
    LOGD(TAG, "writeKey: " + writeKey);
  }
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
  LOGD(TAG, "opeing file from SD Card in write mode");
  File file = SD.open(fileName, FILE_WRITE);
  serializeJson(configJson, file);
}

void updatePOI()
{
  if (!SD.begin(5))
  {
    LOGD(TAG, "SD Card Mount Failed");
    // SD.end();
    return;
  }
  // const size_t CAPACITY = JSON_ARRAY_SIZE(500);
  // DynamicJsonDocument poiIndexJson(CAPACITY);
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
    MySdCard::writeFile(SPIFFS, "/poi/index.json", indexJsonStr.c_str());
    DeserializationError error = deserializeJson(poiIndexJson, indexJsonStr);
    if (error)
    {
      LOGD(TAG, "deserializeJson() failed");
      LOGD(TAG, "error description: " + String(error.f_str()));
      SD.end();
      return;
    }
    else
    {
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
    }
  }
  else
  {
    LOGD(TAG, "GET index.json failed, HTTP Response code: " + String(httpResponseCode));
  }
  // Free resources
  http.end();
  SD.end();
  return;
}

/*
WiFiMulti wifiMulti;
*/

void sleep(int sec)
{
  WiFi.disconnect(true);
  
  M5.Axp.SetLed(0);
  M5.Axp.SetLcdVoltage(0);
  M5.Axp.DeepSleep(SLEEP_SEC(sec));
}

void myDeepSleep(int sec) //link error
{
  WiFi.disconnect(true);

  M5.Axp.SetLed(0);
  M5.Axp.SetLcdVoltage(0);
  M5.Axp.DeepSleep(SLEEP_SEC(sec));
}

void setup()
{
  M5.begin(); // Init M5Core2.
  M5.Lcd.setTextFont(2);
  Serial.begin(9600); // Standard hardware serial port

  // LITTLEFS
  LOGD(TAG, "mounting SPIFFS");
  if (!LittleFS.begin(true))
  {
    LOGD(TAG, "SPIFFS mount failed");
    return;
  }
  else
  {
    LOGD(TAG, "SPIFFS mount done");
  }

  // load config.json from SD
  loadConfig();
  if (rebootCount > rebootLimit)
  {
    rebootCount = 0;
    configJson["rebootCount"] = 0;
    saveConfig();
    LOGD(TAG, "going to deep sleep because of reboot limit.....");
    delay(3000);
    PowerSaving::enable();
    sleep(deepSleepTimeSec);
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

  // static IP address setup
  const IPAddress local_IP(192, 168, 0, 45);
  const IPAddress gateway(192, 168, 0, 1);
  const IPAddress DNS(192, 168, 0, 1);
  const IPAddress subnet(255, 255, 255, 0);
  if (!WiFi.config(local_IP, gateway, subnet, DNS))
  {
    LOGD(TAG, "Failed to configure!");
  }

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
            { request->send(LittleFS, "/index.html"); });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/favicon.ico"); });

  server.on("/justgage/raphael.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/raphael.min.js"); });

  server.on("/justgage/justgage.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/justgage.js"); });

  server.on("/log.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/log.txt"); });

  server.on("/poi/index.json", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/poi/index.json"); });

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
    serializeJson(jsonObj, jsonStr);
    LOGD(TAG, "posted json: " + jsonStr);
    MyBLE::ctrlCommand = 1;
    MyBLE::commandParam = (byte)jsonObj["chargeStatus"] + (byte)jsonObj["dischargeStatus"] * 2;

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
  ambient.begin(channelId, writeKey.c_str(), &wifiClient);
  LOGD(TAG, "ambient setup done");

  saveConfig();
  // setup BLE
  LOGD(TAG, "going to setup BLE");
  MyBLE::bleStartup();
  LOGD(TAG, "BLE setup done");

  // initalize pack volt not to disconnect WiFi
  MyBLE::packBasicInfo.Volts = 15000;
  // ambientlLastSent = millis() + 100000;
  ambientlLastSent = 0;
  LOGD(TAG, "ambientlLastSent initial value: " + String(ambientlLastSent));

  // esp_sleep_enable_timer_wakeup(deepSleepTimeSec * uS_TO_S_FACTOR);
  // LOGD(TAG, "Setup ESP32 to sleep for " + String(deepSleepTimeSec) + " Seconds");

  // MQTT setup
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
  // mqttClient.setServer(mqtt_server.c_str(), mqtt_port); // Sets the server details.
  // mqttClient.setCallback(MyMqtt::callback);  // Sets the message callback function.
  // mqttClient.setCallback(callback); // Sets the message callback function.
  // MyMqtt::server = mqtt_server;
  MyMqtt::setup(&mqttClient, mqtt_server, mqtt_port, mqtt_topic);

  // Button setup
  PowerSaving::setup();

  PowerSaving::enable();
}

void loop()
{
  PowerSaving::loop();

  MyBLE::bleRequestData();
  if (MyBLE::newPacketReceived == true)
  {
    LOGD(TAG, "newPacketReceived == true");
    DISABLE_LOGD = true;
    MyBLE::printBasicInfo();
    DISABLE_LOGD = false;
    LOGD(TAG, "Pack Voltage: " + String(MyBLE::packBasicInfo.Volts));
    DISABLE_LOGD = true;
    LOGD(TAG, "BalanceCodeLow: " + String(MyBLE::packBasicInfo.BalanceCodeLow));
    LOGD(TAG, "MosfetStatus: " + String(MyBLE::packBasicInfo.MosfetStatus));
    LOGD(TAG, "CellAvg: " + String(MyBLE::packCellInfo.CellAvg));
    LOGD(TAG, "CellMedian: " + String(MyBLE::packCellInfo.CellMedian));
    MyBLE::printCellInfo();
    DISABLE_LOGD = false;
  }
  if (MyBLE::packBasicInfo.Volts <= sleepVoltageMv && WiFi.isConnected())
  {
    String logStr = "disconnecting WiFi, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + " <= " + String(sleepVoltageMv);
    LOGD(TAG, logStr);
    LOGLCD(TAG, logStr);
    WiFi.disconnect(true);
    delay(3000);
    PowerSaving::enable();
    // digitalWrite(WIFI_LED, LOW);
    ambientSendIntervalMs = ambientSendIntervalBaseMs * 10;
  }
  else
  {
    if (WiFi.isConnected())
    {
      if (!MyMqtt::connected())
      {
        MyMqtt::reConnect();
        // reConnect();
      }
      // mqttClient.loop();
      MyMqtt::loop();
    }
  }
  if (MyBLE::packBasicInfo.Volts > wakeUpVoltageMv && !WiFi.isConnected())
  {
    wifiConnect();
    LOGD(TAG, "woke up and WiFi reconnected, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + " > " + String(sleepVoltageMv));
    ambientSendIntervalMs = ambientSendIntervalBaseMs;
  }
  //
  if ((millis() - ambientlLastSent) >= ambientSendIntervalMs)
  {
    LOGD(TAG, "millis() - ambientlLastSent: " + String(millis()) + " - " + String(ambientlLastSent) + " >= ambientSendIntervalMs: " + String(ambientSendIntervalMs));
    if (!WiFi.isConnected())
    {
      wifiConnect();
    }
    ambient.set(1, MyBLE::packBasicInfo.Volts / 1000.0f);
    ambient.set(2, MyBLE::packBasicInfo.Amps / 1000.0f);
    ambient.set(3, MyBLE::packCellInfo.CellDiff / 1.0f);
    // if (numberOfTemperature == 2)
    ambient.set(4, (MyBLE::packBasicInfo.Temp1 + MyBLE::packBasicInfo.Temp2) / 2 / 10.0f);
    // else
    // ambient.set(4, MyBLE::packBasicInfo.Temp1 / 10.0f);
    ambient.send();
    ambientlLastSent = millis();

    String megStr = "{\"batteryVoltage\": " + String(MyBLE::packBasicInfo.Volts) + ", \"batteryCurrent\": " + String(MyBLE::packBasicInfo.Amps) + ", \"batteryTemp1\": " + String(MyBLE::packBasicInfo.Temp1);
    // if (numberOfTemperature == 2)
    
    megStr = megStr + ", \"batteryTemp2\": " + String(MyBLE::packBasicInfo.Temp2);
    megStr = megStr + ", \"chargeStatus\": " + String(MyBLE::packBasicInfo.MosfetStatus & 1);
    megStr = megStr + ", \"dischargeStatus\": " + String((MyBLE::packBasicInfo.MosfetStatus & 2) >> 1) + "}";

    // MQTT publish
    // if (!mqttClient.connected())
    if (!MyMqtt::connected())
    {
      MyMqtt::reConnect();
      // reConnect();
    }
    // mqttClient.loop();
    // mqttClient.publish(("stat/" + mqtt_topic + "STATE").c_str(), megStr.c_str());
    MyMqtt::publish("stat/" + mqtt_topic + "STATE", megStr);

    String logStr = "ambient sent, channelId: " + String(channelId) + ", message: " + megStr;
    LOGD(TAG, logStr);
    LOGLCD(TAG, logStr);
    logStr = "MQTT publised, topic: stat/" + mqtt_topic + "STATE";
    logStr = logStr + ", message: " + megStr;
    LOGD(TAG, logStr);
    LOGLCD(TAG, logStr);

    if (MyBLE::packBasicInfo.Volts <= deepSleepVoltageMv)
    {
      String logStr = "Going to deep sleep now and wake up in " + String(deepSleepTimeSec) + " seconds";
      LOGD(TAG, logStr);
      LOGLCD(TAG, logStr);
      delay(2500);
      // esp_deep_sleep_start(); //link error
      // M5.Axp.DeepSleep(SLEEP_SEC(5)); // link error
      PowerSaving::enable();
      sleep(deepSleepTimeSec);
      // myDeepSleep(deepSleepTimeSec); // link error
      // LOGD(TAG, "This will never be printed");
    }
    else
      LOGD(TAG, "PackVoltage: " + String(MyBLE::packBasicInfo.Volts) + " > " + String(deepSleepVoltageMv));
  }
}