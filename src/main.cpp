#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPDateTime.h>
#include <Ambient.h>
#include "MyBLE.hpp"
#include "MyDebug.hpp"

using namespace MyLOG;

//#include <JbdBms.h>
//#include <LittleFS.h>

#define LittleFS SPIFFS

#define WIFI_LED 32

static const String TAG = "main";

// Wi-Fi client
WiFiClient client;

// WiFiMulti
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 10000;

// Web server
AsyncWebServer server(80);

// JbbBms
// JbdBms myBms(&mySerial);
// MyBLE myBLE;

// some varialbles declaration and initalization
// unsigned long SerialLastLoad = 0;
// const int powerMeasurementInterval = 1 * 1000; // milli sec

// packCellInfoStruct cellInfo;

// int batteryTemp1, batteryTemp2, batteryChargePercentage, batteryCurrent, batteryVoltage, cellDiffVoltage, batteryCycleCount, mosFet, cellBalance;
bool cellBalanceList[4];
bool chargeStatus, dischargeStatus;
// bool cellBalance1, cellBalance2, cellBalance3, cellBalance4;

// Ambient service
// const unsigned int channelId = 50366; //Jun BMS (ESP32)
unsigned int channelId = 8630; // Battery Power Meter
// const char *writeKey = "ccb476294fe16acd";
String writeKey = "b473180b50bf1709";
unsigned long ambientlLastSent = 0;
unsigned int ambientSendIntervalBaseMs = 60 * 1000; // milli sec
unsigned int ambientSendIntervalMs = ambientSendIntervalBaseMs;
Ambient ambient;

// sleep control
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
  Serial.println("scan done");
  if (n == 0)
  {
    Serial.println("no networks found");
  }
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
}

void wifiConnect()
{
  Serial.println("Connecting Wifi...");
  // if the connection to the stongest hotstop is lost, it will connect to the next network on the list
  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED)
  {
    String logText = "WiFi connected: " + WiFi.SSID();
    logText += " " + String(WiFi.RSSI());
    LOGD(TAG, logText);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED, HIGH);
  }
  else
  {
    Serial.println("WiFi not connected!");
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
  jsonStr += ", \"chargeStatus\": ";
  chargeStatus = MyBLE::packBasicInfo.MosfetStatus & 1;
  jsonStr += String(chargeStatus);
  jsonStr += ", \"dischargeStatus\": ";
  dischargeStatus = MyBLE::packBasicInfo.MosfetStatus & 1 << 1;
  jsonStr += String(dischargeStatus);
  jsonStr += ", \"batteryList\": [";
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
  MyBLE::disconnectFromServer();
  return "OK";
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
  File fileHandle = LittleFS.open("/config.json", "r");
  if (fileHandle)
  {
    String jsonStr = fileHandle.readStringUntil('\n');
    LOGD(TAG, "config.json: " + jsonStr);
    fileHandle.close();

    // Allocate the JSON document
    StaticJsonDocument<256> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, jsonStr);

    // Test if parsing succeeds.
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else
    {
      int channelId_ = doc["ambient"]["channelId"];
      if (channelId_)
        channelId = channelId_;
      const char *writeKey_ = doc["ambient"]["writeKey"];
      if (writeKey_)
        writeKey = writeKey_;
      int sleepVoltageMv_ = doc["sleepVoltageMv"];
      if (sleepVoltageMv_)
        sleepVoltageMv = sleepVoltageMv_;
      int wakeUpVoltageMv_ = doc["wakeUpVoltageMv"];
      if (wakeUpVoltageMv_)
        wakeUpVoltageMv = wakeUpVoltageMv_;
    }
  }
  
  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("JunBMS");
  // Add list of wifi networks
  wifiMulti.addAP("Jun-Home-AP", "takehiro");
  wifiMulti.addAP("Jun-FS020W", "takehiro");
  wifiMulti.addAP("Jun-Moto-Z2-Play", "takehiro");
  LOGD(TAG, "going to scann WiFi");
  wifiScann();
  LOGD(TAG, "going to connect WiFi");
  wifiConnect();
  LOGD(TAG, "WiFi setup done");

  // setup DateTime
  LOGD(TAG, "Going to setup date");
  setupDateTime();

  // setup webAPIs
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html"); });

  server.on("/justgage/raphael.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/raphael.min.js"); });

  server.on("/justgage/justgage.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/justgage.js"); });

  server.on("/log.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/log.txt"); });

  server.on("/getValues", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", getValues().c_str()); });

  server.on("/disconnectBLE", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", disconnectBLE().c_str()); });

  server.begin();

  // init ambient channelID and key
  ambient.begin(channelId, writeKey.c_str(), &client);
  LOGD(TAG, "ambient setup done");

  // setup BLE
  MyBLE::bleStartup();
  LOGD(TAG, "BLE setup done");

  // initalize pack volt not to disconnect WiFi
  MyBLE::packBasicInfo.Volts = 15000;
}

void loop()
{
  MyBLE::bleRequestData();
  if (MyBLE::newPacketReceived == true)
  {
    LOGD(TAG, "new pcaket received");
    // showInfoLcd;
    MyBLE::printBasicInfo();
    DISABLE_LOGD = true;
    LOGD(TAG, "Pack Voltage: " + String(MyBLE::packBasicInfo.Volts));
    LOGD(TAG, "BalanceCodeLow: " + String(MyBLE::packBasicInfo.BalanceCodeLow));
    LOGD(TAG, "MosfetStatus: " + String(MyBLE::packBasicInfo.MosfetStatus));
    LOGD(TAG, "CellAvg: " + String(MyBLE::packCellInfo.CellAvg));
    LOGD(TAG, "CellMedian: " + String(MyBLE::packCellInfo.CellMedian));
    DISABLE_LOGD = false;
    MyBLE::printCellInfo();
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