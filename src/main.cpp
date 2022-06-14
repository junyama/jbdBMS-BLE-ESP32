#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ESPDateTime.h>
#include <Ambient.h>
#include "MyBLE.hpp"
#include "MyDebug2.hpp"

using namespace MyLOG;

//#include <JbdBms.h>
//#include <LittleFS.h>

#define LittleFS SPIFFS

#define BLUELED 32

static const String TAG_ = "main";
//static const String TAG = "main"; // error: redefinition of 'const String TAG'

// Wi-Fi client
WiFiClient client;

// WiFiMulti
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 10000;

// Web server
AsyncWebServer server(80);

// JbbBms
// JbdBms myBms(&mySerial);
MyBLE myBLE;

// some varialbles declaration and initalization
// unsigned long SerialLastLoad = 0;
const int powerMeasurementInterval = 1 * 1000; // milli sec

// packCellInfoStruct cellInfo;

// int batteryTemp1, batteryTemp2, batteryChargePercentage, batteryCurrent, batteryVoltage, cellDiffVoltage, batteryCycleCount, mosFet, cellBalance;
bool cellBalanceList[4];
bool chargeStatus, dischargeStatus;
// bool cellBalance1, cellBalance2, cellBalance3, cellBalance4;

// Ambient service
// const unsigned int channelId = 50366; //Jun BMS (ESP32)
const unsigned int channelId = 8630; // Battery Power Meter
// const char *writeKey = "ccb476294fe16acd";
const char *writeKey = "b473180b50bf1709";
unsigned long ambientlLastSent = 0;
const unsigned int ambientSendIntervalBase = 60 * 1000; // milli sec
unsigned int ambientSendInterval = ambientSendIntervalBase;
Ambient ambient;

// sleep control
float sleepVoltage = 13.199 * 1000; // mV

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
    LOGD2(TAG_, "DateTime setup done");
  else
    LOGD2(TAG_, "Failed to get time from server.");
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
    LOGD2(TAG_, logText);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(BLUELED, HIGH);
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
  jsonStr += String(myBLE.myClientCallback->BLE_client_connected);
  //jsonStr += String(BLE_client_connected);
  jsonStr += "}";
  return jsonStr;
}

String disconnectBLE()
{
  myBLE.disconnectFromServer();
  return "OK";
}

void setup()
{
  Serial.begin(115200); // Standard hardware serial port

  // LED setup
  pinMode(BLUELED, OUTPUT);
  digitalWrite(BLUELED, LOW);

  // LITTLEFS
  LOGD2(TAG_, "mounting LittleFS");
  if (!LittleFS.begin(true))
  {
    LOGD2(TAG_, "SPIFFS mount failed");
    return;
  }
  else
  {
    LOGD2(TAG_, "SPIFFS mount done");
  }

  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("JunBMS");
  // Add list of wifi networks
  wifiMulti.addAP("Jun-Home-AP", "takehiro");
  wifiMulti.addAP("Jun-FS020W", "takehiro");
  wifiMulti.addAP("Jun-Moto-Z2-Play", "takehiro");
  LOGD2(TAG_, "going to scann WiFi");
  wifiScann();
  LOGD2(TAG_, "going to connect WiFi");
  wifiConnect();
  LOGD2(TAG_, "WiFi setup done");

  // setup DateTime
  LOGD2(TAG_, "Going to setup date");
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
  ambient.begin(channelId, writeKey, &client);
  LOGD2(TAG_, "ambient setup done");

  // setup BLE
  myBLE.bleStartup();
  LOGD2(TAG_, "BLE setup done");

  // initalize pack volt not to disconnect WiFi
  MyBLE::packBasicInfo.Volts = 15000;
}

void loop()
{
  myBLE.bleRequestData();
  if (MyBLE::newPacketReceived == true)
  {
    LOGD2(TAG_, "new pcaket received");
    // showInfoLcd;
    myBLE.printBasicInfo();
    LOGD2(TAG_, "Pack Voltage: " + String(MyBLE::packBasicInfo.Volts));
    LOGD2(TAG_, "BalanceCodeLow: " + String(MyBLE::packBasicInfo.BalanceCodeLow));
    LOGD2(TAG_, "MosfetStatus: " + String(MyBLE::packBasicInfo.MosfetStatus));
    LOGD2(TAG_, "CellAvg: " + String(MyBLE::packCellInfo.CellAvg));
    LOGD2(TAG_, "CellMedian: " + String(MyBLE::packCellInfo.CellMedian));
    myBLE.printCellInfo();
  }
  if (MyBLE::packBasicInfo.Volts <= sleepVoltage && WiFi.isConnected())
  {
    LOGD2(TAG_, "disconnecting WiFi, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + " <= " + String(sleepVoltage));
    WiFi.disconnect(true);
    digitalWrite(BLUELED, LOW);
    ambientSendInterval = ambientSendIntervalBase * 10;
  }
  if (MyBLE::packBasicInfo.Volts > sleepVoltage && !WiFi.isConnected())
  {
    wifiConnect();
    LOGD2(TAG_, "woke up and WiFi reconnected, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + " > " + String(sleepVoltage));
    ambientSendInterval = ambientSendIntervalBase;
  }
  if (millis() - ambientlLastSent >= ambientSendInterval)
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
    LOGD2(TAG_, "ambient sent, batteryVoltage: " + String(MyBLE::packBasicInfo.Volts) + ", batteryCurrent: " + String(MyBLE::packBasicInfo.Amps) + ", batteryTemp1: " + String(MyBLE::packBasicInfo.Temp1) + ", batteryTemp2: " + String(MyBLE::packBasicInfo.Temp2));
  }
}