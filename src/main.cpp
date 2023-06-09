#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPDateTime.h>
#include <Ambient.h>
#include "MyBLE.hpp"
#include "MyDebug.hpp"
#include "MySdCard.hpp"
#include <HTTPClient.h>

using namespace MyLOG;

// #include <JbdBms.h>
// #include <LittleFS.h>

#define LittleFS SPIFFS
#define CONFIG_FILE "config.json"

#define WIFI_LED 32

static const String TAG = "main";

StaticJsonDocument<512> configJson;

// SD Card
// MySdCard mySdCard;

// Wi-Fi client
WiFiClient client;

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
float sleepVoltage = 13.399 * 1000;  // mV
unsigned int sleepVoltageMv = 13199; // mV
unsigned int wakeUpVoltageMv = 13399;

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
    LOGD(TAG, "DateTime setup done");
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
  // if the connection to the stongest hotstop is lost, it will connect to the next network on the list
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED)
  {
    String logText = "WiFi connected: " + WiFi.SSID();
    logText += " " + String(WiFi.RSSI());
    LOGD(TAG, logText);
    logText = "IP: ";
    logText += String(WiFi.localIP());
    LOGD(TAG, logText);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED, HIGH);
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
  jsonStr += String(MyBLE::packBasicInfo.Temp2);
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
  dischargeStatus = MyBLE::packBasicInfo.MosfetStatus & 1 << 1;
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

void updatePOI()
{
  if (!SD.begin(5))
  {
    LOGD(TAG, "SD Card Mount Failed");
    // SD.end();
    return;
  }
  const size_t CAPACITY = JSON_ARRAY_SIZE(500);
  DynamicJsonDocument poiIndexJson(CAPACITY);
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

void setup()
{
  Serial.begin(115200); // Standard hardware serial port

  // LED setup
  pinMode(WIFI_LED, OUTPUT);
  digitalWrite(WIFI_LED, LOW);

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

  // loading configuration from a file
  // Allocate the JSON document
  // StaticJsonDocument<512> configJson;
  String fileName = "/";
  fileName += CONFIG_FILE;
  File fileHandle = LittleFS.open(fileName, "r");
  if (fileHandle)
  {
    String jsonStr = fileHandle.readStringUntil('\n');
    LOGD(TAG, fileName + ": " + jsonStr);
    fileHandle.close();

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(configJson, jsonStr);

    // Test if parsing succeeds.
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      LOGD(TAG, error.f_str());
    }
    else
    {
      int channelId_ = configJson["ambient"]["channelId"];
      if (channelId_)
        channelId = channelId_;
      const char *writeKey_ = configJson["ambient"]["writeKey"];
      if (writeKey_)
        writeKey = writeKey_;
      int sleepVoltageMv_ = configJson["sleepVoltageMv"];
      if (sleepVoltageMv_)
        sleepVoltageMv = sleepVoltageMv_;
      int wakeUpVoltageMv_ = configJson["wakeUpVoltageMv"];
      if (wakeUpVoltageMv_)
        wakeUpVoltageMv = wakeUpVoltageMv_;
    }
  }

  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("JunBMS");

  // static IP address setup
  const IPAddress local_IP(192, 168, 0, 145);
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
  LOGD(TAG, "going to scann WiFi");
  wifiScann();
  LOGD(TAG, "going to connect WiFi");
  if (wifiConnect() != 0)
  {
    LOGD(TAG, "failed to connect WiFi and exiting");
    exit(-1);
  }
  //

 /*
  const char *ssid = "Jun-Home-AP";
  const char *password = "takehiro";
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  */

  LOGD(TAG, "WiFi setup done");

  // setup DateTime
  LOGD(TAG, "Going to setup date");
  setupDateTime();

  // update POI in SD card
  updatePOI();

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
  ambient.begin(channelId, writeKey.c_str(), &client);
  LOGD(TAG, "ambient setup done");

  // setup BLE
  MyBLE::bleStartup();
  LOGD(TAG, "BLE setup done");

  // initalize pack volt not to disconnect WiFi
  MyBLE::packBasicInfo.Volts = 15000;
  ambientlLastSent = millis();
}

void loop()
{
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
    LOGD(TAG, "disconnecting WiFi, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + " <= " + String(sleepVoltageMv));
    WiFi.disconnect(true);
    digitalWrite(WIFI_LED, LOW);
    ambientSendIntervalMs = ambientSendIntervalBaseMs * 10;
  }
  if (MyBLE::packBasicInfo.Volts > wakeUpVoltageMv && !WiFi.isConnected())
  {
    wifiConnect();
    LOGD(TAG, "woke up and WiFi reconnected, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + " > " + String(sleepVoltageMv));
    ambientSendIntervalMs = ambientSendIntervalBaseMs;
  }
  if (millis() - ambientlLastSent >= ambientSendIntervalMs)
  {
    if (!WiFi.isConnected())
    {
      wifiConnect();
    }
    ambient.set(1, MyBLE::packBasicInfo.Volts / 1000.0f);
    ambient.set(2, MyBLE::packBasicInfo.Amps / 1000.0f);
    ambient.set(3, MyBLE::packCellInfo.CellDiff / 1.0f);
    ambient.set(4, (MyBLE::packBasicInfo.Temp1 + MyBLE::packBasicInfo.Temp2) / 2 / 10.0f);
    ambient.send();
    ambientlLastSent = millis();
    LOGD(TAG, "ambient sent, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + ", batteryCurrent: " + String(MyBLE::packBasicInfo.Amps) + ", batteryTemp1: " + String(MyBLE::packBasicInfo.Temp1) + ", batteryTemp2: " + String(MyBLE::packBasicInfo.Temp2));
  }
}