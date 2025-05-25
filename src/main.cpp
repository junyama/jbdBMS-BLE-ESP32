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
// #include "MyAmbient2.hpp"
//  #include "MyMqtt2.hpp"
#include <StreamUtils.h>

#include "PowerSaving2.hpp"
#include "MyLcd2.hpp"

#include "VoltMater.hpp"
#include "LipoMater.hpp"

using namespace MyLOG;

#define CONFIG_FILE "config.json"

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */

static const String TAG = "main";

JsonDocument configJson;
// DeserializationError error = deserializeJson(configJson, "{\"numberOfTemperature\": 2, \"sleepVoltageMv\": 12999, \"wakeUpVoltageMv\": 13899, \"deepSleepVoltageMv\": 11699, \"deepSleepTimeSec\": 900, \"wifi\": [{\"ssid\": \"Jun-Home-AP\", \"pass\": \"xxxxx\"}, {\"ssid\": \"Jun-FS020W\", \"pass\": \"xxxxx\"}], \"poiURL\": \"http://junichi2.ddns.net/\", \"ambient\": {\"channelId\": 50366, \"writeKey\": \"ccb476294fe16acd\", \"ambientSendIntervalBaseMs\": 60000}}");

// Wi-Fi client
WiFiClient wifiClient;

// WiFiMulti
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 20000;

// sleep control
unsigned int numberOfTemperature = 2; // numbe of temperature sensor
// float sleepVoltage = 13.399 * 1000;   // mV
// unsigned int sleepVoltageMv = 13199;  // mV
// unsigned int wakeUpVoltageMv = 13399;
// unsigned int deepSleepVoltageMv = 13199; // mV
unsigned int deepSleepTimeSec = 900; // seconds
unsigned int rebootCount = 0;
unsigned int rebootLimit = 10;

// BLE
MyBLE2 myBleArr[3];
// String macArr[2];
JsonArray deviceList;
// int numberOfDevices = 2;
int numberOfBleDevices = 0;
//  bool cellBalanceList[4];
bool chargeStatus, dischargeStatus;

// Volt Mater
VoltMater voltMater;

// Lipo Mater
LipoMater lipoMater;

// MQTT
// MyMqtt2 mqttClient2;
String mqttServer = "192.168.0.20";
String mqttServer2 = "junichi.ddns.net";
int mqttPort = 1883;
String mqttUser = "mqtt-user";
String mqttPass = "mqttpass";
String systemTopic = "junichiM5Core2/";
// String lipoTopic = "junichiLipo_0/";
PubSubClient mqttClient(wifiClient);
bool mqttDisabled = false;
int mqttMessageSizeLimit = 128;
void reConnectMqttServer();

// Ambient
// MyAmbient2 ambientClient2;

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
  myLcd.println("Connecting Wifi..."); // Serial port format output string.
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

String disconnectBLE(int bleIndex)
{
  myBleArr[bleIndex].ctrlCommand = 2;
  return "OK";
}

String requestDeviceName(int bleIndex)
{
  myBleArr[bleIndex].ctrlCommand = 3;
  return "OK";
}

String getDeviceName(int bleIndex)
{
  return myBleArr[bleIndex].deviceName;
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
  myLcd.println("loading config file...");
  LOGD(TAG, "configJsonText: " + textStr);
  DeserializationError error = deserializeJson(configJson, textStr.c_str());
  if (error)
  {
    LOGD(TAG, "Deserialization error.");
    return;
  }

  deviceList = configJson["devices"].as<JsonArray>();
  // numberOfDevices = deviceList.size();
  // LOGD(TAG, "number of devices: " + String(numberOfDevices));
  /*
  int bleIndex = 0;
  for (int i = 0; i < deviceList.size(); i++)
  {
    JsonDocument deviceObj = deviceList[i];
    String deviceType = deviceObj["type"];
    if (deviceType.equals("BMS"))
    {
      String mac = deviceObj["mac"];
      JsonDocument obj;
      obj["mac"] = mac;
      macArr[bleIndex] = obj;
      bleIndex++;
    }
  }
  numberOfBleDevices = bleIndex;
  LOGD(TAG, "number of BLE devices: " + String(numberOfBleDevices));
  */

  /*
  for (int deviceIndex = 0; deviceIndex < numberOfBleDevices; deviceIndex++)
  {
    String topic = deviceList[deviceIndex]["mqtt"]["topic"];
    myBleArr[deviceIndex].deviceTopic = topic; //does not work
    LOGD(TAG, "setting myBleArr[" + String(deviceIndex) + "].deviceTopic " + myBleArr[deviceIndex].deviceTopic);
  }
  */

  /*
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
  */
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
      myLcd.println("Updating POI...");

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
      myLcd.println("POI update done!");
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

/*
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

JsonDocument getAllState()
{
  JsonDocument doc;
  String jsonStr;
  // JsonDocument deviceObj;
  JsonArray deviceList = doc.to<JsonArray>();
  // LOGD(TAG, "numberOfBleDevices: " + String(numberOfBleDevices));
  for (int bleIndex = 0; bleIndex < numberOfBleDevices; bleIndex++)
  {
    JsonDocument deviceObj;
    deviceObj["deviceName"] = myBleArr[bleIndex].deviceNameStr;
    deviceObj["batteryVoltage"] = String(myBleArr[bleIndex].packBasicInfo.Volts / 1000.0);
    deviceObj["batteryCurrent"] = String(myBleArr[bleIndex].packBasicInfo.Amps / 1000.0);
    deviceObj["batteryTemp1"] = String(myBleArr[bleIndex].packBasicInfo.Temp1 / 10.0);
    if (myBleArr[bleIndex].numberOfTemperature == 2)
      deviceObj["batteryTemp2"] = String(myBleArr[bleIndex].packBasicInfo.Temp2 / 10.0);
    deviceObj["batteryChargePercentage"] = String(myBleArr[bleIndex].packBasicInfo.CapacityRemainPercent);
    deviceObj["chargeStatus"] = String(myBleArr[bleIndex].packBasicInfo.MosfetStatus & 1);
    deviceObj["dischargeStatus"] = String((myBleArr[bleIndex].packBasicInfo.MosfetStatus & 2) >> 1);
    // serializeJson(deviceObj, jsonStr);
    // LOGD(TAG, "deviceObj: " + jsonStr);
    deviceList.add(deviceObj);
  }
  // serializeJson(deviceList, jsonStr);
  // LOGD(TAG, "deviceList: " + jsonStr);
  / *
  JsonDocument voltMaterObj = voltMater.getVoltage();
  {
    JsonDocument deviceObj;
    deviceObj["calVoltage"] = voltMaterObj["calVoltage"];
    deviceList.add(deviceObj);
  }
  * /
  {
    // JsonDocument deviceObj;
    // deviceObj["lipoVoltage"] = String(lipoMater.getVoltage());
    // deviceObj["lipoCurrent"] = String(lipoMater.getCurrent());
    deviceList.add(lipoMater.getState());
  }
  // doc["devices"] = deviceList;
  serializeJsonPretty(doc, jsonStr);
  LOGD(TAG, "getAllState() returns: " + jsonStr);
  return doc;
}
*/

/*
JsonDocument getBmsState(int bleIndex)
{
  JsonDocument doc;
  doc["deviceName"] = myBleArr[bleIndex].deviceNameStr;
  doc["batteryVoltage"] = String(myBleArr[bleIndex].packBasicInfo.Volts / 1000.0);
  doc["batteryCurrent"] = String(myBleArr[bleIndex].packBasicInfo.Amps / 1000.0);
  doc["batteryTemp1"] = String(myBleArr[bleIndex].packBasicInfo.Temp1 / 10.0);
  if (configJson["numberOfTemperature"] == 2)
    doc["batteryTemp2"] = String(myBleArr[bleIndex].packBasicInfo.Temp2 / 10.0);
  doc["batteryChargePercentage"] = String(myBleArr[bleIndex].packBasicInfo.CapacityRemainPercent);
  doc["chargeStatus"] = String(myBleArr[bleIndex].packBasicInfo.MosfetStatus & 1);
  doc["dischargeStatus"] = String((myBleArr[bleIndex].packBasicInfo.MosfetStatus & 2) >> 1);
  // JsonDocument doc2 = voltMater.getVoltage();
  // doc["calVoltage"] = doc2["calVoltage"];
  // doc["lipoVoltage"] = String(M5.Axp.GetBatVoltage());
  // doc["lipoCurrent"] = String(M5.Axp.GetBatCurrent());
  return doc;
}
*/

/*
JsonDocument getLipoState()
{
  JsonDocument doc;
  deviceObj["lipoVoltage"] = String(lipoMater.getVoltage());
  deviceObj["lipoCurrent"] = String(lipoMater.getCurrent());
  return doc;
}
*/

// MQTT functions definitions
String getDeviceTopic(int deviceIndex)
{
  JsonDocument deviceObj = deviceList[deviceIndex];
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
      String topicStr = "stat/" + systemTopic + "STATE";
      publish(topicStr.c_str(), "MQTT reconnected");
      // ... and resubscribe.
      topicStr = "cmnd/" + systemTopic + "#";
      mqttClient.subscribe(topicStr.c_str());
      LOGD(TAG, "topic(" + topicStr + ") subscribed");
      for (int deviceIndex = 0; deviceIndex < deviceList.size(); deviceIndex++)
      {
        // JsonDocument deviceObj = deviceList[deviceIndex];
        // String deviceTopic = deviceObj["mqtt"]["topic"];
        topicStr = "cmnd/" + getDeviceTopic(deviceIndex) + "#";
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
  for (int deviceIndex = 0; deviceIndex < deviceList.size(); deviceIndex++)
  {
    JsonDocument deviceObj = deviceList[deviceIndex];
    String deviceTopic = deviceObj["mqtt"]["topic"];
    discoveryTopic = "homeassistant/device/" + deviceTopic + "config";
    discoveryPayload = deviceObj["mqtt"]["discoveryPayload"];
    LOGD(TAG, String(deviceIndex) + ": publishing for HA discovery.................");
    publishJson(discoveryTopic, discoveryPayload, true);
  }
}

void publishHaDiscovery2(JsonDocument deviceObj)
{
  String discoveryTopic = deviceObj["mqtt"]["topic"];
  discoveryTopic = "homeassistant/device/" + discoveryTopic + "config";
  JsonDocument discoveryPayload = deviceObj["mqtt"]["discoveryPayload"];
  publishJson(discoveryTopic, discoveryPayload, true);
}

void mqttCallback(char *topic_, byte *payload, unsigned int length)
{
  LOGD(TAG, "mqttCallback invoked.");

  String msgStr = "";
  for (int i = 0; i < length; i++)
  {
    msgStr = msgStr + (char)payload[i];
  }
  String logStr = "Message arrived[";
  logStr += topic_;
  logStr += "] ";
  logStr += msgStr;
  LOGD(TAG, logStr);
  LOGLCD(TAG, logStr);

  if (String(topic_).equals("cmnd/" + voltMater.topic + "getState"))
  {
    LOGD(TAG, "responding to geState of " + voltMater.topic);
    publishJson(("stat/" + voltMater.topic + "RESULT").c_str(), voltMater.getState(), false);
    return;
  }

  if (String(topic_).equals("cmnd/" + lipoMater.topic + "getState"))
  {
    LOGD(TAG, "responding to geState of " + lipoMater.topic);
    publishJson(("stat/" + lipoMater.topic + "RESULT").c_str(), lipoMater.getState(), false);
    return;
  }

  int chargeStatus;
  int dischargeStatus;
  bool connectionStatus;
  for (int bleIndex = 0; bleIndex < numberOfBleDevices; bleIndex++)
  {
    LOGD(TAG, "myBleArr[" + String(bleIndex) + "].topic = " + myBleArr[bleIndex].topic);
    LOGD(TAG, "myBleArr[" + String(bleIndex) + "].available = " + String(myBleArr[bleIndex].available));
    if (!myBleArr[bleIndex].available)
    {
      LOGD(TAG, "myBleArr[" + String(bleIndex) + "] is not available so continue.");
      continue;
    }
    String deviceTopic = myBleArr[bleIndex].topic;
    chargeStatus = myBleArr[bleIndex].packBasicInfo.MosfetStatus & 1;
    dischargeStatus = (myBleArr[bleIndex].packBasicInfo.MosfetStatus & 2) >> 1;
    connectionStatus = myBleArr[bleIndex].isConnected();

    if (String(topic_).equals("cmnd/" + deviceTopic + "getBmsState"))
    {
      LOGD(TAG, "responding to getBmsState of " + deviceTopic + "!");
      publishJson(("stat/" + deviceTopic + "RESULT").c_str(), myBleArr[bleIndex].getState(), false);
      return;
    }

    if (String(topic_).equals("cmnd/" + deviceTopic + "connection"))
    {
      LOGD(TAG, "connection status: " + String(connectionStatus));
      if (msgStr.equals(""))
      {
        if (connectionStatus)
          msgStr = "ON";
        else
          msgStr = "OFF";
      }
      else if (msgStr.equals("0"))
      {
        myBleArr[bleIndex].disconnectFromServer();
        msgStr = "OFF";
        connectionStatus = false;
      }
      else if (msgStr.equals("1"))
      {
        myBleArr[bleIndex].connectToServer();
        msgStr = "ON";
        connectionStatus = true;
      }
      else
      {
        msgStr = "INVALID";
      }
      LOGD(TAG, "responding to connection");
      publish(("stat/" + deviceTopic + "CONNECTION").c_str(), msgStr.c_str());
      msgStr = "{\"connectionStatus\": " + String(connectionStatus) + "}";
      publish(("stat/" + deviceTopic + "RESULT").c_str(), msgStr.c_str());
      publish(("stat/" + deviceTopic + "STATE").c_str(), msgStr.c_str());
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
          myBleArr[bleIndex].mosfetCtrl(0, dischargeStatus);
          chargeStatus = 0;
          msgStr = "OFF";
        }
        else if (msgStr.equals("1"))
        {
          myBleArr[bleIndex].mosfetCtrl(1, dischargeStatus);
          chargeStatus = 1;
          msgStr = "ON";
        }
        else if (msgStr.equals("toggle"))
        {
          myBleArr[bleIndex].mosfetCtrl((chargeStatus ^ 1), dischargeStatus);
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
          myBleArr[bleIndex].mosfetCtrl(chargeStatus, 0);
          dischargeStatus = 0;
          msgStr = "OFF";
        }
        else if (msgStr.equals("1"))
        {
          myBleArr[bleIndex].mosfetCtrl(chargeStatus, 1);
          dischargeStatus = 1;
          msgStr = "ON";
        }
        else if (msgStr.equals("toggle"))
        {
          myBleArr[bleIndex].mosfetCtrl(chargeStatus, (dischargeStatus ^ 1));
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

void detectButton()
{
  M5.update(); // Read the press state of the key.
  if (M5.BtnA.wasReleased() || M5.BtnA.pressedFor(1000, 200))
  {
    if (powerSaving.lcdState)
    {
      powerSaving.enable();
    }
    else
    {
      powerSaving.disable();
    }
  }
  else if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(1000, 200))
  {
    if (++myLcd.bmsIndexShown > 2)
      myLcd.bmsIndexShown = 0;
    LOGD(TAG, "Button B pushed with bmsIndex: " + String(myLcd.bmsIndexShown));
    myLcd.showBatteryInfo();
  }
  else if (M5.BtnC.wasReleased() || M5.BtnC.pressedFor(1000, 200))
  {
    M5.shutdown();
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
    publish("stat/" + systemTopic + "STATE", logStr);
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

  // ambientClient2.begin(configJson, &wifiClient);

  // MQTT setup
  myLcd.println("MQTT setting up");
  // mqttClient2.setup(&wifiClient, &myBleArr[0], myBleArr, &voltMater, configJson);
  // void MyMqtt2::setup(WiFiClient *wifiClient, MyBLE2 *myBLE_, MyBLE2 *myBleArr_, VoltMater *voltMater_, JsonDocument configJson_)
  LOGD(TAG, "Setting MQTT parameters ..........");
  String systemTopicConf = configJson["mqtt"]["topic"];
  LOGD(TAG, "configJson[\"mqtt\"][\"topic\"]: " + systemTopicConf);
  if (systemTopicConf != "null")
  {
    systemTopic = systemTopicConf;
    LOGD(TAG, "System topic changed: " + systemTopic);
  }
  else
    LOGD(TAG, "default System topic: " + systemTopic);
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
    systemTopic = mqttTopicConf;
    LOGD(TAG, "MQTT changed systemTopic: " + systemTopic);
  }
  else
    LOGD(TAG, "MQTT default Topic: " + systemTopic);
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
  // myLcd.println("Publishing HA discvery.");
  // publishHaDiscovery();

  myLcd.println("MQTT setup done!");

  // myBLE.configJson = &configJson;
  // myBLE.packBasicInfo.Volts = 15000;
  // myBLE.bleStartup();
  //
  LOGD(TAG, "going to setup each device of deviceList");
  int bleIndex = 0;
  // bool bleServerNotFound = true;
  for (int deviceIndex = 0; deviceIndex < deviceList.size(); deviceIndex++)
  {
    Serial.printf("\nSet up device[%d] ===== BEGIN ====================================================\n", deviceIndex);
    JsonDocument deviceObj = deviceList[deviceIndex];
    String type = deviceObj["type"];
    String topic = getDeviceTopic(deviceIndex);
    int numberOfTemperature = deviceObj["numberOfTemperature"];
    if (type.equals("BMS"))
    {
      //  setup BLE
      String logStr = "Setting up BLE(" + String(bleIndex) + ")...";
      LOGD(TAG, logStr);
      myLcd.println(logStr);

      new (myBleArr + bleIndex) MyBLE2(deviceObj);
      myBleArr[bleIndex].bleStartup();

      myLcd.bmsInfoArr[bleIndex].deviceName = myBleArr[bleIndex].deviceName + " (" + myBleArr[bleIndex].mac + ")";

      /*
      JsonDocument deviceStatus = myBleArr[bleIndex].getDeviceStatus();
      String jsonStr;
      serializeJson(deviceStatus, jsonStr);
      LOGD(TAG, "myBleArr[" + String(bleIndex) + "].getDeviceStatus(): " + jsonStr);

      int doConnect = deviceStatus["doConnect"];
      if (doConnect == 0)
      {
        LOGD(TAG, "myBleArr[bleIndex].available = false");
        myLcd.println("BLE(" + String(bleIndex) + ") is not available");
      }
      else
      {
        myLcd.println("BLE(" + String(bleIndex) + ") is connected");
        LOGD(TAG, "myBleArr[bleIndex].available = true");
      }
      */
      myBleArr[bleIndex].available = myBleArr[bleIndex].myAdvertisedDeviceCallbacks->doConnect;
      //
      if (myBleArr[bleIndex].available)
      {
        // LOGD(TAG, "getting BLE device name");
        // String deviceName = myBleArr[bleIndex].getDeviceNameLoop();
        // myLcd.bmsInfoArr[bleIndex].deviceName = deviceName + " (" + myBleArr[bleIndex].mac + ")";
        publishHaDiscovery2(deviceObj);
        // myBleArr[bleIndex].getDeviceNameLoop();
        // bleServerNotFound = false; //not effective for connectionStatus issue
        // LOGD(TAG, "BLE Server found exit scan --->>>>>>>>");
      }
      bleIndex++;
    }
    else if (type.equals("VAMater"))
    {
      voltMater.setup(deviceObj);
      // if (topic != "null")
      // voltMater.topic = topic;
      if (voltMater.available)
        publishHaDiscovery2(deviceObj);
      LOGD(TAG, "Volt Mater setup done");
      myLcd.println("Volt Mater setup done!");
    }
    else if (type.equals("Lipo"))
    {
      lipoMater.setup(deviceObj);
      // if (topic != "null")
      // lipoMater.topic = topic;
      if (lipoMater.available)
        publishHaDiscovery2(deviceObj);
      LOGD(TAG, "Lipo Mater setup done");
      myLcd.println("Lipo Mater setup done!");
    }
    Serial.printf("Set up device[%d] ===== END ==========================================================\n\n", deviceIndex);
  }
  numberOfBleDevices = bleIndex;
  LOGD(TAG, "number of BLE devices: " + String(numberOfBleDevices));

  for (int bleIndex = 0; bleIndex < numberOfBleDevices; bleIndex++)
  {
    JsonDocument deviceStatus = myBleArr[bleIndex].getDeviceStatus();
    String topic = deviceStatus["topic"];
    topic = "stat/" + topic + "STATE";
    publishJson(topic, deviceStatus, true);
  }

  //
  // LOGD(TAG, "getting device name....");
  // getDeviceNameLoop(&myBLE);
  // myBLE.getDeviceNameLoop();

  LOGD(TAG, "Device setup done");
  myLcd.println("Device setup done!");

  // esp_sleep_enable_timer_wakeup(deepSleepTimeSec * uS_TO_S_FACTOR);
  // LOGD(TAG, "Setup ESP32 to sleep for " + String(deepSleepTimeSec) + " Seconds");

  // Button setup
  // PowerSaving::setup();
  powerSaving.setup();

  myLcd.println("ALL setup done!");
  myLcd.println("enabling power save.");
  delay(2000);
  // PowerSaving::enable();
  powerSaving.enable();
}
/////////////////////////////
void loop()
{
  // powerSaving.loop();
  // myLcd.loop();
  detectButton();

  if (!mqttClient.connected())
  {
    reConnectMqttServer();
  }
  mqttClient.loop();

  //int bleIndex = 0;
  for (int bleIndex = 0; bleIndex < numberOfBleDevices; bleIndex++)
  {
    //JsonDocument deviceObj = deviceList[deviceIndex];
    //String type = deviceObj["type"];
    // String topic = getDeviceTopic(deviceIndex);
    //String topic = deviceObj["mqtt"]["topic"];
    //if (type.equals("BMS"))
    {
      // LOGD(TAG, "going to bleRequestData()");
      if (myBleArr[bleIndex].available)
      {
        myBleArr[bleIndex].bleRequestData();
        if (myBleArr[bleIndex].newPacketReceived == true)
        {
          LOGD(TAG, "newPacketReceived == true");
          /*
          DISABLE_LOGD = true;
          myBleArr[bleIndex].printBasicInfo();
          DISABLE_LOGD = false;
          LOGD(TAG, "Pack Voltage: " + String(myBleArr[bleIndex].packBasicInfo.Volts));
          DISABLE_LOGD = true;
          LOGD(TAG, "BalanceCodeLow: " + String(myBleArr[bleIndex].packBasicInfo.BalanceCodeLow));
          LOGD(TAG, "MosfetStatus: " + String(myBleArr[bleIndex].packBasicInfo.MosfetStatus));
          LOGD(TAG, "CellAvg: " + String(myBleArr[bleIndex].packCellInfo.CellAvg));
          LOGD(TAG, "CellMedian: " + String(myBleArr[bleIndex].packCellInfo.CellMedian));
          myBleArr[bleIndex].printCellInfo();
          DISABLE_LOGD = false;
          */

          // JsonDocument deviceObj = deviceList[bleIndex];
          // String deviceTopic = deviceObj["mqtt"]["topic"];
          publishJson("stat/" + myBleArr[bleIndex].topic + "STATE", myBleArr[bleIndex].getState(), true);
          // publishJson("stat/" + systemTopic + "STATE", voltMater.getVoltage(), true);
          // voltMater.lastMeasurment = millis();

          myLcd.updateBmsInfo(bleIndex, myBleArr[bleIndex].packBasicInfo.Volts / 1000.0f,
                              myBleArr[bleIndex].packBasicInfo.Amps / 1000.0f,
                              myBleArr[bleIndex].packCellInfo.CellDiff / 1.0f,
                              myBleArr[bleIndex].packBasicInfo.Temp1 / 10.0f,
                              myBleArr[bleIndex].packBasicInfo.Temp2 / 10.0f,
                              myBleArr[bleIndex].packBasicInfo.CapacityRemainPercent);
        }
        else if (myBleArr[bleIndex].timeout(millis()))
        {
          myLcd.updateBmsInfo(bleIndex, myBleArr[bleIndex].packBasicInfo.Volts / 1000.0f,
                              myBleArr[bleIndex].packBasicInfo.Amps / 1000.0f,
                              myBleArr[bleIndex].packCellInfo.CellDiff / 1.0f,
                              myBleArr[bleIndex].packBasicInfo.Temp1 / 10.0f,
                              myBleArr[bleIndex].packBasicInfo.Temp2 / 10.0f,
                              myBleArr[bleIndex].packBasicInfo.CapacityRemainPercent);
          publishJson("stat/" + myBleArr[bleIndex].topic + "STATE", myBleArr[bleIndex].getState(), true);
          myBleArr[bleIndex].lastMeasurment = millis();
        }
        //myBleArr[bleIndex].disconnectFromServer(); //Jun: added, but a reconnect does not work.
      }
      //bleIndex++;
    }
  }
  if (voltMater.available && voltMater.timeout(millis()))
  {
    myLcd.updateVoltMaterInfo(voltMater.calVoltage);
    publishJson("stat/" + voltMater.topic + "STATE", voltMater.getState(), true);
    voltMater.lastMeasurment = millis();
  }
  if (lipoMater.available && lipoMater.timeout(millis()))
  {
    myLcd.updateLipoInfo();
    publishJson("stat/" + lipoMater.topic + "STATE", lipoMater.getState(), true);
    lipoMater.lastMeasurment = millis();
  }
  // delay(100);
}