#include <WiFi.h>
#include <WiFiMulti.h>
#include "ESPAsyncWebServer.h"

#include <MyBLE.cpp>
//#include "MyBMS.h"

//#include <JbdBms.h>
//#include <LittleFS.h>
#include "SPIFFS.h"
#define LittleFS SPIFFS
#include <Ambient.h>
#include "ESPDateTime.h"

#define BLUELED 32

// debug cotrol
const bool debugCtrl = false;
const int verbose = 2;
// const String TAG = "main";

// Wi-Fi client
// const char *ssidList[] = {"Jun-Home-AP", "Jun-FS020W"};
//const char *ssidList[] = {"Jun-Home-AP", "Jun-FS020W"};
//const char *password = "takehiro"; // WIFI password
WiFiClient client;

// WiFiMulti
WiFiMulti wifiMulti;
const uint32_t connectTimeoutMs = 10000;

// Web server
AsyncWebServer server(80);

// JbbBms
// JbdBms myBms(&mySerial);
MyBLE myBms;

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
float sleepVoltage = 13.299 * 1000; // mV

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
    /*
    Serial.print("WiFi connected: ");
    Serial.print(WiFi.SSID());
    Serial.print(" ");
    Serial.println(WiFi.RSSI());
    */
    String logText = "WiFi connected: " + WiFi.SSID();
    logText += " " + String(WiFi.RSSI());
    LOGD(TAG, logText);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(BLUELED, HIGH);
  }
  else
  {
    Serial.println("WiFi not connected!");
  }
}

/*
void wifiConnect2()
{
  for (unsigned int i = 0; i < sizeof(ssidList); i++)
  {
    LOGD(TAG, "Connecting to " + String(ssidList[i]));
    WiFi.begin(ssidList[i], password);

    // try to connect AP in 10 sec
    for (int j = 1; j < 10; j++)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("");
        LOGD(TAG, "WiFi connected");
        break;
      }
      delay(1000);
      Serial.print(".");
    }
    if (WiFi.isConnected())
    {
      if (verbose > 1)
      {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        // LOGD(TAG, "IP: " + String(WiFi.localIP())); does not show 4 octets format
      }
      digitalWrite(BLUELED, HIGH);
      break;
    }
    LOGD(TAG, "");
    LOGD(TAG, "Timeout");
  }
}
*/

String getValues()
{
  String jsonStr = "";
  jsonStr.reserve(300);
  jsonStr += "{\"batteryTemp1\": ";
  jsonStr += String(packBasicInfo.Temp1);
  jsonStr += ", \"batteryTemp2\": ";
  jsonStr += String(packBasicInfo.Temp2);
  jsonStr += ", \"batteryChargePercentage\": ";
  jsonStr += String(packBasicInfo.CapacityRemainPercent);
  jsonStr += ", \"batteryCurrent\": ";
  jsonStr += String(packBasicInfo.Amps / 10);
  // jsonStr += ", \"batteryCycleCount\": ";
  // jsonStr += String(batteryCycleCount);
  jsonStr += ", \"batteryVoltage\": ";
  jsonStr += String(packBasicInfo.Volts / 10);
  jsonStr += ", \"chargeStatus\": ";
  chargeStatus = packBasicInfo.MosfetStatus & 1;
  jsonStr += String(chargeStatus);
  jsonStr += ", \"dischargeStatus\": ";
  dischargeStatus = packBasicInfo.MosfetStatus & 1 << 1;
  jsonStr += String(dischargeStatus);
  jsonStr += ", \"batteryList\": [";
  jsonStr += String(packCellInfo.CellVolt[0]);
  for (int i = 1; i < packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(packCellInfo.CellVolt[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"batteryDiff\": ";
  jsonStr += String(packCellInfo.CellDiff);
  for (int i = 0; i < packCellInfo.NumOfCells; i++)
  {
    cellBalanceList[i] = packBasicInfo.BalanceCodeLow & 1 << i;
  }
  jsonStr += ", \"cellBalanceList\": [";
  jsonStr += String(cellBalanceList[0]);
  for (int i = 1; i < packCellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellBalanceList[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"cellMedian\": ";
  jsonStr += String(packCellInfo.CellMedian);
  jsonStr += ", \"BLEConnected\": ";
  jsonStr += String(BLE_client_connected);
  jsonStr += "}";
  return jsonStr;
}

String disconnectBLE()
{
  myBms.disconnectFromServer();
  return "OK";
}

void setup()
{
  Serial.begin(9600); // Standard hardware serial port

  // LED setup
  pinMode(BLUELED, OUTPUT);
  digitalWrite(BLUELED, LOW);

  // LITTLEFS
  LOGD(TAG, "mounting LittleFS");
  if (!LittleFS.begin(true))
  {
    LOGD(TAG, "LittleFS mount failed");
    return;
  }
  else
  {
    LOGD(TAG, "mounting succeeded");
    String logText = "Logging started, debug: " + String(debugCtrl);
    logText += ", verbose: " + String(verbose);
    LOGD(TAG, logText);
    LOGD(TAG, "LittleFS mount done");
  }

  // Rand()
  if (debugCtrl)
    LOGD(TAG, "RAND_MAX = " + String(RAND_MAX));

  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("JunBMS");
  // Add list of wifi networks
  wifiMulti.addAP("Jun-Home-AP", "takehiro");
  wifiMulti.addAP("Jun-FS020W", "takehiro");
  wifiMulti.addAP("Jun-Moto-Z2-Play", "takehiro");
  LOGD(TAG, "going to scann WiFi");
  wifiScann();
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
  ambient.begin(channelId, writeKey, &client);
  LOGD(TAG, "ambient setup done");

  // setup BLE
  bmsSerial.begin(9600, SERIAL_8N1, 21, 22);
  myBms.bleStartup();
  LOGD(TAG, "BLE setup done");

  // initalize pack volt not to disconnect WiFi
  packBasicInfo.Volts = 15000;
}

void loop()
{
  myBms.bleRequestData();
  if (newPacketReceived == true)
  {
    LOGD(TAG, "new pcaket received");
    // showInfoLcd;
    myBms.printBasicInfo();
    LOGD(TAG, "Pack Voltage: " + String(packBasicInfo.Volts));
    LOGD(TAG, "BalanceCodeLow: " + String(packBasicInfo.BalanceCodeLow));
    LOGD(TAG, "MosfetStatus: " + String(packBasicInfo.MosfetStatus));
    LOGD(TAG, "CellAvg: " + String(packCellInfo.CellAvg));
    LOGD(TAG, "CellMedian: " + String(packCellInfo.CellMedian));
    myBms.printCellInfo();
  }
  if (packBasicInfo.Volts <= sleepVoltage && WiFi.isConnected())
  {
    LOGD(TAG, "disconnecting WiFi, batteryVoltage: " + String(packBasicInfo.Volts) + " <= " + String(sleepVoltage));
    WiFi.disconnect(true);
    digitalWrite(BLUELED, LOW);
    ambientSendInterval = ambientSendIntervalBase * 10;
  }
  if (packBasicInfo.Volts > sleepVoltage && !WiFi.isConnected())
  {
    wifiConnect();
    LOGD(TAG, "woke up and WiFi reconnected, batteryVoltage: " + String(packBasicInfo.Volts) + " > " + String(sleepVoltage));
    ambientSendInterval = ambientSendIntervalBase;
  }
  if (millis() - ambientlLastSent >= ambientSendInterval)
  {
    if (!WiFi.isConnected())
    {
      wifiConnect();
    }
    ambient.set(1, packBasicInfo.Volts / 1000.0f);
    ambient.set(2, packBasicInfo.Amps / 1000.0f);
    ambient.set(3, packCellInfo.CellDiff / 1.0f);
    ambient.set(4, (packBasicInfo.Temp1 + packBasicInfo.Temp2) / 2 / 10.0f);
    ambient.send();
    ambientlLastSent = millis();
    LOGD(TAG, "ambient sent, batteryVoltage: " + String(packBasicInfo.Volts) + ", batteryCurrent: " + String(packBasicInfo.Amps) + ", batteryTemp1: " + String(packBasicInfo.Temp1) + ", batteryTemp2: " + String(packBasicInfo.Temp2));
  }
}