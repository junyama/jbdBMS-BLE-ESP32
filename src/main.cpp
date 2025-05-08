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
// #include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPDateTime.h>
#include <Ambient.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

#include "MyBLE2.hpp"

#include "MyDebug.hpp"
#include "MySdCard.hpp"
#include "MyAmbient2.hpp"
// #include "MyMqtt2.hpp"
#include <StreamUtils.h>

#include "PowerSaving2.hpp"
#include "MyLcd2.hpp"

#include "VoltMater.hpp"

using namespace MyLOG;

#define CONFIG_FILE "config.json"

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */

static const String TAG = "main";

JsonDocument configJson;
DeserializationError error = deserializeJson(configJson, "{\"numberOfTemperature\": 2, \"sleepVoltageMv\": 12999, \"wakeUpVoltageMv\": 13899, \"deepSleepVoltageMv\": 11699, \"deepSleepTimeSec\": 900, \"wifi\": [{\"ssid\": \"Jun-Home-AP\", \"pass\": \"xxxxx\"}, {\"ssid\": \"Jun-FS020W\", \"pass\": \"xxxxx\"}], \"poiURL\": \"http://junichi2.ddns.net/\", \"ambient\": {\"channelId\": 50366, \"writeKey\": \"ccb476294fe16acd\", \"ambientSendIntervalBaseMs\": 60000}}");

// Wi-Fi client
WiFiClient wifiClient;

// WiFiMulti
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 20000;

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
MyBLE2 myBleArr[2];
JsonArray deviceList;
int numberOfBleDevices = 2;
// bool cellBalanceList[4];
bool chargeStatus, dischargeStatus;

// Volt Mater
VoltMater voltMater;

// MQTT
// MyMqtt2 mqttClient2;
String mqttServer = "192.168.0.20";
String mqttServer2 = "junichi.ddns.net";
int mqttPort = 1883;
String mqttUser = "mqtt-user";
String mqttPass = "mqttpass";
PubSubClient mqttClient(wifiClient);
bool mqttDisabled = false;
int mqttMessageSizeLimit = 128;
String hostTopic = "junichiM5Core2/";
void reConnectMqttServer();

// Ambient
MyAmbient2 ambientClient2;

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
    LOGD(TAG, "Failed to get time from mqttServer.");
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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
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

  deviceList = configJson["devices"].as<JsonArray>();
  // numberOfBleDevices = deviceList.size();
  numberOfBleDevices = 0;
  for (int i = 0; i < deviceList.size(); i++)
  {
    JsonDocument deviceObj = deviceList[i];
    String deviceType = deviceObj["type"];
    if (deviceType.equals("BLE"))
      numberOfBleDevices++;
  }
  LOGD(TAG, "number of BLE devices: " + String(numberOfBleDevices));

  /*
  for (int deviceId = 0; deviceId < numberOfBleDevices; deviceId++)
  {
    String topic = deviceList[deviceId]["mqtt"]["topic"];
    myBleArr[deviceId].deviceTopic = topic; //does not work
    LOGD(TAG, "setting myBleArr[" + String(deviceId) + "].deviceTopic " + myBleArr[deviceId].deviceTopic);
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
  // numberOfBleDevices = configJson["devices"].size();
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

JsonDocument getBmsState(int deviceId)
{
  JsonDocument doc;
  doc["deviceName"] = myBleArr[deviceId].deviceNameStr;
  doc["batteryVoltage"] = String(myBleArr[deviceId].packBasicInfo.Volts / 1000.0);
  doc["batteryCurrent"] = String(myBleArr[deviceId].packBasicInfo.Amps / 1000.0);
  doc["batteryTemp1"] = String(myBleArr[deviceId].packBasicInfo.Temp1 / 10.0);
  if (configJson["numberOfTemperature"] == 2)
    doc["batteryTemp2"] = String(myBleArr[deviceId].packBasicInfo.Temp2 / 10.0);
  doc["batteryChargePercentage"] = String(myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
  doc["chargeStatus"] = String(myBleArr[deviceId].packBasicInfo.MosfetStatus & 1);
  doc["dischargeStatus"] = String((myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1);
  JsonDocument doc2 = voltMater.getVoltage();
  doc["calVoltage"] = doc2["calVoltage"];
  doc["lipoVoltage"] = String(M5.Axp.GetBatVoltage());
  doc["lipoCurrent"] = String(M5.Axp.GetBatCurrent());
  doc["sleepVoltage"] = configJson["sleepVoltage"];
  doc["wakeUpVoltageMv"] = configJson["wakeUpVoltageMv"];
  doc["deepSleepVoltageMv"] = configJson["deepSleepVoltageMv"];
  doc["deepSleepTimeSec"] = configJson["deepSleepTimeSec"];
  doc["ambientSendIntervalBaseMs"] = configJson["ambient"]["ambientSendIntervalBaseMs"];
  return doc;
}

JsonDocument getState(int deviceId)
{
  JsonDocument doc;
  doc["deviceName"] = myBleArr[deviceId].deviceNameStr;
  doc["batteryVoltage"] = String(myBleArr[deviceId].packBasicInfo.Volts / 1000.0);
  doc["batteryCurrent"] = String(myBleArr[deviceId].packBasicInfo.Amps / 1000.0);
  doc["batteryTemp1"] = String(myBleArr[deviceId].packBasicInfo.Temp1 / 10.0);
  if (configJson["numberOfTemperature"] == 2)
    doc["batteryTemp2"] = String(myBleArr[deviceId].packBasicInfo.Temp2 / 10.0);
  doc["batteryChargePercentage"] = String(myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
  doc["chargeStatus"] = String(myBleArr[deviceId].packBasicInfo.MosfetStatus & 1);
  doc["dischargeStatus"] = String((myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1);
  //JsonDocument doc2 = voltMater.getVoltage();
  //doc["calVoltage"] = doc2["calVoltage"];
  doc["lipoVoltage"] = String(M5.Axp.GetBatVoltage());
  doc["lipoCurrent"] = String(M5.Axp.GetBatCurrent());
  return doc;
}

// MQTT functions definitions
String getDeviceTopic(int deviceId)
{
  JsonDocument deviceObj = deviceList[deviceId];
  String deviceTopic = deviceObj["mqtt"]["topic"];
  return deviceTopic;
}

void publish(String topic, String message)
{
  if (mqttDisabled)
    return;
  LOGD(TAG, "publishing >>>>> topic: " + topic + ", message: " + message);
  if (!mqttClient.connected())
  {
    reConnectMqttServer();
  }
  for (int i = 0; i < message.length(); i = i + mqttMessageSizeLimit)
  {
    mqttClient.publish(topic.c_str(), message.substring(i, i + mqttMessageSizeLimit).c_str());
  }
}

void publishJson(String topic, JsonDocument doc, bool retained)
{
  if (mqttDisabled)
    return;
  String jsonStr;
  serializeJson(doc, jsonStr);
  LOGD(TAG, "publishing Json >>>>> topic: " + topic + ", payload: " + jsonStr);
  if (!mqttClient.connected())
  {
    reConnectMqttServer();
  }
  mqttClient.beginPublish(topic.c_str(), measureJson(doc), retained);
  BufferingPrint bufferedClient(mqttClient, 32);
  serializeJson(doc, bufferedClient);
  bufferedClient.flush();
  mqttClient.endPublish();
}

void reConnectMqttServer()
{
  LOGD(TAG, "reConnectMqttServer() called");
  int i = 1;
  while (!mqttClient.connected())
  {
    LOGD(TAG, "Attempting MQTT connection...");
    // Create a random client ID.
    String clientId = "M5Stack-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect.
    bool isConnected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str());
    LOGD(TAG, "mqttUser: " + mqttUser + ", mqttPass: " + mqttPass);
    // if (client->connect(clientId.c_str()))
    if (isConnected)
    {
      LOGD(TAG, "Connected.");
      // Once connected, publish an announcement to the topic.
      String topicStr = "stat/" + hostTopic + "STATE";
      publish(topicStr.c_str(), "MQTT reconnected");
      // ... and resubscribe.
      topicStr = "cmnd/" + hostTopic + "#";
      mqttClient.subscribe(topicStr.c_str());
      LOGD(TAG, "topic(" + topicStr + ") subscribed");
      for (int deviceId = 0; deviceId < numberOfBleDevices; deviceId++)
      {
        // JsonDocument deviceObj = deviceList[deviceId];
        // String deviceTopic = deviceObj["mqtt"]["topic"];
        topicStr = "cmnd/" + getDeviceTopic(deviceId) + "#";
        mqttClient.subscribe(topicStr.c_str());
        LOGD(TAG, "topic(" + topicStr + ") subscribed");
      }
      // Home aAssistant discoverry
      // publishHaDiscovery(); //this makes reconnect fail loop
      return;
    }
    else
    {
      String logStr = String(i) + ": ";
      logStr += "failed reconnecting, rc = ";
      logStr += mqttClient.state();
      LOGD(TAG, logStr);
      if (i == 5)
      {
        LOGD(TAG, "failed to reConnectMqttServer many times.");
        mqttDisabled = true;
        reset();
      }
      if (i == 3)
      {
        LOGD(TAG, "failed to reConnectMqttServer a few times. Use the second mqttServer");
        String mqttServerConf2 = configJson["mqtt"]["server2"];
        if (mqttServerConf2 != "null")
        {
          mqttServer = mqttServerConf2;
          LOGD(TAG, "MQTT changed mqttServer: " + mqttServer);
        }
        mqttClient.setServer(mqttServer.c_str(), mqttPort);
      }
      i++;
      LOGD(TAG, "try to reconnect again in 1 seconds");
      delay(1000);
    }
  }
  return;
}

void publishHaDiscovery()
{
  String discoveryTopic;
  JsonDocument discoveryPayload;
  // JsonArray deviceList = configJson["devices"].as<JsonArray>();
  LOGD(TAG, "loading discovery payload from config.json");
  for (int deviceId = 0; deviceId < numberOfBleDevices; deviceId++)
  {
    JsonDocument deviceObj = deviceList[deviceId];
    String deviceTopic = deviceObj["mqtt"]["topic"];
    discoveryTopic = "homeassistant/device/" + deviceTopic + "config";
    discoveryPayload = deviceObj["mqtt"]["discoveryPayload"];
    LOGD(TAG, "publishing for HA discovery......");
    publishJson(discoveryTopic, discoveryPayload, true);
  }
}

void mqttCallback(char *topic_, byte *payload, unsigned int length)
{
  LOGD(TAG, "mqttCallback invoked.");
  int chargeStatus;
  int dischargeStatus;

  String msgStr = "";
  for (int i = 0; i < length; i++)
  {
    msgStr = msgStr + (char)payload[i];
  }
  String logStr = "Message arrived[";
  logStr = logStr + topic_;
  logStr = logStr + "] ";
  logStr = logStr + msgStr;
  LOGD(TAG, logStr);
  LOGLCD(TAG, logStr);

  // String deviceTopic;
  for (int deviceId = 0; deviceId < numberOfBleDevices; deviceId++)
  {
    chargeStatus = myBleArr[deviceId].packBasicInfo.MosfetStatus & 1;
    dischargeStatus = (myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1;

    // deviceTopic = myBleArr[deviceId].deviceTopic; //does not work
    // LOGD(TAG, "myBleArr[" + String(deviceId) + "].deviceTopic: " + myBleArr[deviceId].deviceTopic);
    // JsonDocument deviceObj = deviceList[deviceId];
    // String deviceTopic = deviceObj["mqtt"]["topic"];
    String deviceTopic = getDeviceTopic(deviceId);

    if (String(topic_).equals("cmnd/" + deviceTopic + "getState"))
    {
      LOGD(TAG, "responding to getState!");
      publishJson(("stat/" + deviceTopic + "RESULT").c_str(), getState(deviceId), false);
      return;
    }
    if (String(topic_).equals("cmnd/" + deviceTopic + "getBmsState"))
    {
      LOGD(TAG, "responding to getBmsState!");
      publishJson("stat/" + deviceTopic + "RESULT", getBmsState(deviceId), false);
      publishJson("stat/" + deviceTopic + "STATE", getBmsState(deviceId), false);
      return;
    }
    if ((String(topic_).equals("cmnd/" + deviceTopic + "charge")) || ((String(topic_).equals("cmnd/" + deviceTopic + "discharge"))))
    {
      if (String(topic_).equals("cmnd/" + deviceTopic + "charge"))
      {
        LOGD(TAG, "charge status: " + String(chargeStatus) + ", discharge status: " + String(dischargeStatus));
        if (msgStr.equals(""))
        {
          if (chargeStatus)
            msgStr = "ON";
          else
            msgStr = "OFF";
        }
        else if (msgStr.equals("0"))
        {
          myBleArr[deviceId].mosfetCtrl(0, dischargeStatus);
          chargeStatus = 0;
          msgStr = "OFF";
        }
        else if (msgStr.equals("1"))
        {
          myBleArr[deviceId].mosfetCtrl(1, dischargeStatus);
          chargeStatus = 1;
          msgStr = "ON";
        }
        else if (msgStr.equals("toggle"))
        {
          myBleArr[deviceId].mosfetCtrl((chargeStatus ^ 1), dischargeStatus);
          chargeStatus = chargeStatus ^ 1;
          msgStr = "TOGGLE";
        }
        else
        {
          msgStr = "INVALID";
        }
        LOGD(TAG, "responding to charge!");
        publish(("stat/" + deviceTopic + "CHARGE").c_str(), msgStr.c_str());
      }
      else if (String(topic_).equals("cmnd/" + deviceTopic + "discharge"))
      {
        LOGD(TAG, "charge status: " + String(chargeStatus) + ", discharge status: " + String(dischargeStatus));
        if (msgStr.equals(""))
        {
          if (dischargeStatus)
            msgStr = "ON";
          else
            msgStr = "OFF";
        }
        else if (msgStr.equals("0"))
        {
          myBleArr[deviceId].mosfetCtrl(chargeStatus, 0);
          dischargeStatus = 0;
          msgStr = "OFF";
        }
        else if (msgStr.equals("1"))
        {
          myBleArr[deviceId].mosfetCtrl(chargeStatus, 1);
          dischargeStatus = 1;
          msgStr = "ON";
        }
        else if (msgStr.equals("toggle"))
        {
          myBleArr[deviceId].mosfetCtrl(chargeStatus, (dischargeStatus ^ 1));
          dischargeStatus = dischargeStatus ^ 1;
          msgStr = "TOGGLE";
        }
        else
        {
          msgStr = "INVALID";
        }
        LOGD(TAG, "responding to discharge!");
        publish(("stat/" + deviceTopic + "DISCARGE").c_str(), msgStr.c_str());
      }
      msgStr = "{\"chargeStatus\": " + String(chargeStatus) + ", \"dischargeStatus\": " + String(dischargeStatus) + "}";
      publish(("stat/" + deviceTopic + "RESULT").c_str(), msgStr.c_str());
      publish(("stat/" + deviceTopic + "STATE").c_str(), msgStr.c_str());
      return;
    }
  }
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
    publish("stat/" + hostTopic + "STATE", logStr);
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

  ambientClient2.begin(configJson, &wifiClient);

  // MQTT setup
  M5.Lcd.println("MQTT setting up!");
  // mqttClient2.setup(&wifiClient, &myBleArr[0], myBleArr, &voltMater, configJson);
  // void MyMqtt2::setup(WiFiClient *wifiClient, MyBLE2 *myBLE_, MyBLE2 *myBleArr_, VoltMater *voltMater_, JsonDocument configJson_)

  LOGD(TAG, "Setting MQTT parameters ..........");
  String mqttServerConf = configJson["mqtt"]["server"];
  LOGD(TAG, "configJson[\"mqtt\"][\"server\"]: " + mqttServerConf);
  if (mqttServerConf != "null")
  {
    mqttServer = mqttServerConf;
    LOGD(TAG, "MQTT changed server: " + mqttServer);
  }
  else
    LOGD(TAG, "MQTT default server: " + mqttServer);
  int mqttPortConf = configJson["mqtt"]["port"];
  if (mqttPortConf)
  {
    mqttPort = mqttPortConf;
    LOGD(TAG, "MQTT server changed port: " + String(mqttPort));
  }
  else
    LOGD(TAG, "MQTT server default port: " + String(mqttPort));
  String mqttTopicConf = configJson["mqtt"]["topic"];
  LOGD(TAG, "configJson[\"mqtt\"][\"topic\"]: " + mqttTopicConf);
  if (mqttTopicConf != "null")
  {
    hostTopic = mqttTopicConf;
    LOGD(TAG, "MQTT changed hostTopic: " + hostTopic);
  }
  else
    LOGD(TAG, "MQTT default Topic: " + hostTopic);
  String mqttUserConf = configJson["mqtt"]["user"];
  if (mqttUserConf != "null")
  {
    mqttUser = mqttUserConf;
    LOGD(TAG, "MQTT changed User: " + mqttUser);
  }
  else
    LOGD(TAG, "MQTT default User: " + mqttUser);
  String mqttPassConf = configJson["mqtt"]["password"];
  if (mqttPassConf != "null")
  {
    mqttPass = mqttPassConf;
    LOGD(TAG, "MQTT changed Pass: " + mqttPass);
  }
  else
    LOGD(TAG, "MQTT default Pass: " + mqttPass);
  /*
  int messageSizeLimitConf = configJson["mqtt"]["messageSizeLimit"];
  if (messageSizeLimitConf)
  {
      messageSizeLimit = messageSizeLimitConf;
      LOGD(TAG, "MQTT changed messageSizeLimit: " + String(messageSizeLimit));
  }
  else
      LOGD(TAG, "MQTT default messageSizeLimit: " + String(messageSizeLimit));
  */
  mqttClient.setServer(mqttServer.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);

  // Home aAssistant discoverry
  M5.Lcd.println("Publishing HA discvery.");
  publishHaDiscovery();

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
  for (int i = 0; i < numberOfBleDevices; i++)
  {
    Serial.printf("\n\nmyBleArr[%d] =========================================================================\n", i);
    new (myBleArr + i) MyBLE2();
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
  if (!mqttClient.connected())
  {
    reConnectMqttServer();
  }
  mqttClient.loop();
  for (int deviceId = 0; deviceId < numberOfBleDevices; deviceId++)
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

      // JsonDocument deviceObj = deviceList[deviceId];
      // String deviceTopic = deviceObj["mqtt"]["topic"];
      publishJson("stat/" + getDeviceTopic(deviceId) + "STATE", getState(deviceId), true);
      //publishJson("stat/" + hostTopic + "STATE", voltMater.getVoltage(), true);
      //voltMater.lastMeasurment = millis();

      myLcd.showBatteryInfo(myBleArr[deviceId].packBasicInfo.Volts / 1000.0f, myBleArr[deviceId].packBasicInfo.Amps / 1000.0f, myBleArr[deviceId].packCellInfo.CellDiff / 1.0f, myBleArr[deviceId].packBasicInfo.Temp1 / 10.0f, voltMater.calVoltage, myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
    }
    if (myBleArr[deviceId].packBasicInfo.Volts <= sleepVoltageMv && WiFi.isConnected())
    {
      String logStr = "disconnecting WiFi, batteryVoltage: " + String(myBleArr[deviceId].packBasicInfo.Volts) + " <= " + String(sleepVoltageMv);
      LOGD(TAG, logStr);
      publish("stat/" + hostTopic + "STATE", logStr);
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
        if (!mqttClient.connected())
        {
          reConnectMqttServer();
        }
        mqttClient.loop();
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

      //publishJson("stat/" + hostTopic + "STATE", voltMater.getVoltage(), true);
      //voltMater.lastMeasurment = millis();

      if (myBleArr[deviceId].packBasicInfo.Volts <= deepSleepVoltageMv)
      {
        String logStr = "Going to deep sleep now and wake up in " + String(deepSleepTimeSec) + " seconds";
        LOGD(TAG, logStr);
        myLcd.println(logStr);
        publish("stat/" + hostTopic + "STATE", logStr);
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
  }
  if (voltMater.timeout(millis()))
  {
    publishJson("stat/" + hostTopic + "STATE", voltMater.getVoltage(), true);
    voltMater.lastMeasurment = millis();
  }
}