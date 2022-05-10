#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include "ESPAsyncWebServer.h"

#include "FS.h"
#include "SPIFFS.h"
//#include <LITTLEFS.h>

#include <ESPDateTime.h>

#include <JbdBms.h>

#include <Ambient.h>

#define BLUELED 32

#define MYPORT_TX 12
#define MYPORT_RX 13

// debug cotrol
const bool debugCtrl = true;
const int verbose = 2;

// SoftwareSerial
SoftwareSerial myPort;

// Wi-Fi client
const char *ssidList[] = {"Jun-Home-AP", "Jun-FS020W"};
const char *password = "takehiro"; // WIFI password
WiFiClient client;

// Web server
// WebServer server(80);
AsyncWebServer server(80);

// JbbBms
// JbdBms myBms(&Serial);
JbdBms myBms(&myPort);
// JbdBms myBms(0, 1);

// some varialbles initalization
unsigned long SerialLastLoad = 0;
const int powerMeasurementInterval = 1 * 1000; // milli sec
packCellInfoStruct cellInfo;

int batteryTemp1, batteryTemp2, batteryChargePercentage, batteryCurrent, batteryVoltage, batteryCycleCount;

int BatterycycleCount;

// Ambient service
const unsigned int channelId = 8630;
const char *writeKey = "b473180b50bf1709";
unsigned long ambientlLastSent = 0;
unsigned int ambientSendInterval = 60 * 1000; // milli sec
Ambient ambient;

// sleep control
// const int sleepTime = 60 * 1000 * 1000; // micro sec
const float sleepVoltage = -1.0 * 1000; // mV

// local functions definitions
void serialPrint(String text)
{
  if (verbose > 1)
    Serial.print(text);
}

void serialPrintln(String text)
{
  if (verbose > 1)
    Serial.println(text);
}

void logWrite(String text, const char *mode)
{
  if (verbose > 0)
  {
    File fileHandle = SPIFFS.open("/log.txt", mode);
    if (!fileHandle)
      serialPrintln("/log.txt open failed");
    else
    {
      fileHandle.println("[" + DateTime.toString() + "]: " + text);
      fileHandle.close();
    }
  }
}

void setupDateTime()
{
  // setup this after wifi connected
  // you can use custom timeZone,server and timeout
  // DateTime.setTimeZone("CST-8");
  DateTime.setTimeZone("JST-9");
  DateTime.setServer("ntp.gol.com");
  // DateTime.begin(15 * 1000);
  // from
  /** changed from 0.2.x **/
  DateTime.begin(15 * 1000 /* timeout param */);
  if (DateTime.isTimeValid())
    serialPrintln("DateTime setup done");
  else
    serialPrintln("Failed to get time from server.");
}

void wifiConnect()
{
  for (unsigned int i = 0; i < sizeof(ssidList); i++)
  {
    // boolean WiFi.isConnected() = false;
    serialPrint("Connecting to ");
    serialPrintln(ssidList[i]);
    WiFi.begin(ssidList[i], password);

    // try to connect AP in 10 sec
    for (int j = 1; j < 10; j++)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        serialPrintln("");
        serialPrintln("WiFi connected");
        // WiFi.isConnected() = true;
        break;
      }
      delay(1000);
      serialPrint(".");
    }
    if (WiFi.isConnected())
    {
      if (verbose > 1)
      {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
      }
      digitalWrite(BLUELED, HIGH);
      break;
    }
    serialPrintln("");
    serialPrintln("Timeout");
  }
}

String getValues()
{
  String jsonStr = "";
  jsonStr.reserve(300);
  jsonStr += "{\"batteryTemp1\": ";
  jsonStr += String(batteryTemp1);
  jsonStr += ", \"batteryTemp2\": ";
  jsonStr += String(batteryTemp2);
  jsonStr += ", \"batteryChargePercentage\": ";
  jsonStr += String(batteryChargePercentage);
  jsonStr += ", \"batteryCurrent\": ";
  jsonStr += String(batteryCurrent);
  jsonStr += ", \"batteryCycleCount\": ";
  jsonStr += String(batteryCycleCount);
  jsonStr += ", \"batteryVoltage\": ";
  jsonStr += String(batteryVoltage);
  jsonStr += ", \"batteryList\": [";
  jsonStr += String(cellInfo.CellVoltage[0]);
  for (int i = 1; i < cellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellInfo.CellVoltage[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"batteryDiff\": ";
  jsonStr += String(cellInfo.CellDiff);
  jsonStr += "}";
  return jsonStr;
}

void setup()
{
  Serial.begin(9600); // Standard hardware serial port

  // LED setup
  pinMode(BLUELED, OUTPUT);
  digitalWrite(BLUELED, LOW);

  // SPIFFS stup
  if (!SPIFFS.begin(true))
  {
    serialPrintln("An Error has occurred while mounting SPIFFS");
    return;
  }
  else
  {
    serialPrintln("mounting succeeded");
    String logText = "Logging started, debug: " + String(debugCtrl);
    logText += ", verbose: " + String(verbose);
    logWrite(logText, "w");
    logWrite("SPIFFS mount done", "a");
  }

  // Software serial setup
  myPort.begin(38400, SWSERIAL_8N1, MYPORT_RX, MYPORT_TX, false);
  if (!myPort)
  { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid SoftwareSerial pin configuration, check config");
    while (1)
    { // Don't continue with invalid configuration
      delay(1000);
    }
  }

  // Rand()
  serialPrintln("RAND_MAX = " + String(RAND_MAX));

  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("BMS");
  wifiConnect();
  logWrite("WiFi setup done", "a");

  // setup DateTime
  setupDateTime();
  logWrite("DateTime setup done", "a");

  // setup webAPIs
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html"); });

  server.on("/getValues", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", getValues().c_str()); });

  server.on("/justgage/raphael.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/raphael.min.js"); });

  server.on("/justgage/justgage.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/justgage.js"); });

  server.on("/log.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/log.txt"); });

  server.begin();

  // init ambient channelID and key
  ambient.begin(channelId, writeKey, &client);
}

void loop()
{
  // server.handleClient();
  if (millis() - SerialLastLoad >= powerMeasurementInterval)
  {
    SerialLastLoad = millis();
    if (debugCtrl)
    {
      batteryCurrent = rand() % 4000;
      batteryChargePercentage = rand() % 100;
      batteryTemp1 = rand() % 500;
      batteryTemp2 = rand() % 500;
      batteryCycleCount = 10;
      batteryVoltage = rand() % 1600;
      cellInfo.NumOfCells = 4;
      for (int i = 0; i < cellInfo.NumOfCells; i++)
      {
        cellInfo.CellVoltage[i] = rand() % 4000;
      }
      cellInfo.CellDiff = rand() % 1000;
    }
    else if (myBms.readBmsData())
    {
      batteryCurrent = myBms.getCurrent2();

      batteryChargePercentage = myBms.getChargePercentage2();

      batteryTemp1 = myBms.getTemp1_2();

      batteryTemp2 = myBms.getTemp2_2();

      batteryCycleCount = myBms.getCycle();

      batteryVoltage = myBms.getVoltage2();

      serialPrintln("getCurrent(): " + String(myBms.getCurrent2()) + ", getChargePercentage(): " + String(myBms.getChargePercentage2()) + ", getTemp1(): " + String(myBms.getTemp1_2()) + ", getTemp2(): " + String(myBms.getTemp2_2()) + ", getVoltage(): " + String(myBms.getVoltage2()));
      // logWrite("myBms.getVoltage(): " + String(myBms.getVoltage()), "a");
      if (myBms.readPackData() == true)
      {
        cellInfo = myBms.getPackCellInfo();
      }
    }
    else
    {
      serialPrintln("myBms.readBmsData() failed");
      logWrite("myBms.readBmsData() failed", "a");
    }
    if ((batteryVoltage > sleepVoltage) && !WiFi.isConnected())
    {
      wifiConnect();
      logWrite("woke up and WiFi reconnected, batteryVoltage: " + String(batteryVoltage), "a");
    }
  }
  if (millis() - ambientlLastSent >= ambientSendInterval)
  {
    ambient.set(1, batteryVoltage / 100);
    ambient.set(2, batteryCurrent / 100);
    ambient.set(3, batteryTemp1 / 10.0);
    ambient.set(4, batteryTemp2 / 10.0);
    ambient.send();
    ambientlLastSent = millis();
    serialPrintln("ambient sent, batteryVoltage: " + String(batteryVoltage) + ", batteryCurrent: " + String(batteryCurrent) + ", batteryTemp1: " + String(batteryTemp1) + ", batteryTemp2: " + String(batteryTemp2));
    logWrite("ambient sent, batteryVoltage: " + String(batteryVoltage) + ", batteryCurrent: " + String(batteryCurrent) + ", batteryTemp1: " + String(batteryTemp1) + ", batteryTemp2: " + String(batteryTemp2), "a");
    if (batteryVoltage <= sleepVoltage)
    {
      serialPrintln("disconnecting WiFi, batteryVoltage: " + String(batteryVoltage, 0));
      logWrite("disconnecting WiFi, batteryVoltage: " + String(batteryVoltage, 0), "a");
      WiFi.disconnect(true);
      digitalWrite(BLUELED, LOW);
    }
  }
}