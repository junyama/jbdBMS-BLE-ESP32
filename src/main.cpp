/*
 *  if the esp8266 board has usb to serial converter on the board, the hardware serial wont work, software serial must be used.
 *  I used ESP-01S. BMS may have 5v or 3.3v TTL level. Logic level shifter must install when BMS is 5v.
 *  wiring:
 *  [bms]   [step down to 3.3v]      [ESP-01S]
 *  VCC (12V) ---->  VIN+ ----------> 3V3
 *  TX  ----------------------------> RX
 *  RX  ----------------------------> TX
 *  GND  ---->  VIN-    ------------> GND
 *
 *  Please fill the wifi settings.
 *  Run web browser http://IP
 *
 */
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <JbdBms.h>
//#include <LittleFS.h>
#include "SPIFFS.h"
#define LittleFS SPIFFS
#include <Ambient.h>
#include "ESPDateTime.h"

#define BLUELED 32

//#define MY_TX 14
//#define MY_RX 12

// debug cotrol
const bool debugCtrl = false;
const int verbose = 2;

HardwareSerial mySerial(2);
// SoftwareSerial mySerial(MY_RX, MY_TX);

// Wi-Fi client
const char *ssidList[] = {"Jun-Home-AP", "Jun-FS020W"};
const char *password = "takehiro"; // WIFI password
WiFiClient client;

// Web server
AsyncWebServer server(80);

// JbbBms
JbdBms myBms(&mySerial);
// JbdBms myBms(MY_RX, MY_TX);

// some varialbles declaration and initalization
unsigned long SerialLastLoad = 0;
const int powerMeasurementInterval = 1 * 1000; // milli sec
packCellInfoStruct cellInfo;
int batteryTemp1, batteryTemp2, batteryChargePercentage, batteryCurrent, batteryVoltage, cellDiffVoltage, batteryCycleCount, mosFet, cellBalance;
bool cellBalanceList[4];
bool chargeStatus, dischargeStatus;
bool cellBalance1, cellBalance2, cellBalance3, cellBalance4;

// Ambient service
const unsigned int channelId = 50366;
const char *writeKey = "ccb476294fe16acd";
unsigned long ambientlLastSent = 0;
const unsigned int ambientSendIntervalBase = 60 * 1000; // milli sec
unsigned int ambientSendInterval = ambientSendIntervalBase;
Ambient ambient;

// sleep control
float sleepVoltage = 11.0 * 100; // 10mV

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

int logLineCounter = 0;
void logWrite(String text, const char *mode)
{
  if (verbose > 0)
  {
    File fileHandle = LittleFS.open("/log.txt", mode);
    if (!fileHandle)
      serialPrintln("/log.txt open failed");
    else
    {
      fileHandle.println("[" + DateTime.toString() + "]: " + text);
      fileHandle.close();
      logLineCounter++;
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
  jsonStr += ", \"chargeStatus\": ";
  jsonStr += String(chargeStatus);
  jsonStr += ", \"dischargeStatus\": ";
  jsonStr += String(dischargeStatus);
  jsonStr += ", \"batteryList\": [";
  jsonStr += String(cellInfo.CellVoltage[0]);
  for (int i = 1; i < cellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellInfo.CellVoltage[i]);
  }
  jsonStr += "]";
  jsonStr += ", \"batteryDiff\": ";
  jsonStr += String(cellDiffVoltage);
  jsonStr += ", \"cellBalanceList\": [";
  jsonStr += String(cellBalanceList[0]);
  for (int i = 1; i < cellInfo.NumOfCells; i++)
  {
    jsonStr += ", ";
    jsonStr += String(cellBalanceList[i]);
  }
  jsonStr += "]";
  jsonStr += "}";
  return jsonStr;
}

void setup()
{
  Serial.begin(9600); // Standard hardware serial port

  // LED setup
  pinMode(BLUELED, OUTPUT);
  digitalWrite(BLUELED, LOW);

  // LITTLEFS
  serialPrintln("mounting LittleFS");
  if (!LittleFS.begin(true))
  {
    serialPrintln("LittleFS mount failed");
    return;
  }
  else
  {
    serialPrintln("mounting succeeded");
    String logText = "Logging started, debug: " + String(debugCtrl);
    logText += ", verbose: " + String(verbose);
    logWrite(logText, "w");
    logWrite("LittleFS mount done", "a");
  }

  /* mySerial setup
  mySerial.begin(9600);
  if (!mySerial)
  { // If the object did not initialize, then its configuration is invalid
    Serial.println("mySerial setup failed");
    while (1)
    { // Don't continue with invalid configuration
      delay(1000);
    }
  } else Serial.println("mySerial setup done");
  */

  // Rand()
  if (debugCtrl)
    serialPrintln("RAND_MAX = " + String(RAND_MAX));

  // setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("JunBMS");
  wifiConnect();
  logWrite("WiFi setup done", "a");

  // setup DateTime
  setupDateTime();
  logWrite("DateTime setup done", "a");

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

  server.begin();

  // init ambient channelID and key
  ambient.begin(channelId, writeKey, &client);
}

void loop()
{
  if (logLineCounter > 200)
  {
    logLineCounter = 0;
    logWrite("", "w");
  }
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
      batteryChargePercentage = myBms.getChargePercentage();
      batteryTemp1 = myBms.getTemp1_2();
      batteryTemp2 = myBms.getTemp2_2();
      batteryCycleCount = myBms.getCycle();
      batteryVoltage = myBms.getVoltage2();
      mosFet = myBms.getMosfet();
      chargeStatus = mosFet & 0b00000001;
      dischargeStatus = mosFet & 0b00000010;
      cellBalance = myBms.getCellBalance();
      cellBalance1 = cellBalance & 0b00000001;
      cellBalance2 = cellBalance & 0b00000010;
      cellBalanceList[1] = cellBalance & 1 << 1;
      for (int i = 0; i < cellInfo.NumOfCells; i++)
      {
        cellBalanceList[i] = cellBalance & 1 << i;
      }
      serialPrint("Current: " + String(batteryCurrent) + ", ChargePercentage: " + String(batteryChargePercentage));
      serialPrint(", Temp1: " + String(batteryTemp1) + ", Temp2: " + String(batteryTemp2));
      serialPrint(", Voltage: " + String(batteryVoltage));
      serialPrint(", mosFet: " + String(mosFet));
      serialPrint(", chargeStatus: " + String(chargeStatus));
      serialPrint(", dischargeStatus: " + String(dischargeStatus));
      serialPrint(", cellBalance2: " + String(cellBalanceList[1]));
      serialPrintln(", cellBalance: " + String(cellBalance));
      if (myBms.readPackData() == true)
      {
        cellInfo = myBms.getPackCellInfo();
      }
      cellDiffVoltage = cellInfo.CellDiff;
      if (batteryVoltage <= sleepVoltage && WiFi.isConnected())
      {
        serialPrintln("disconnecting WiFi, batteryVoltage: " + String(batteryVoltage, 0));
        logWrite("disconnecting WiFi, batteryVoltage: " + String(batteryVoltage, 0), "a");
        WiFi.disconnect(true);
        digitalWrite(BLUELED, LOW);
        ambientSendInterval = ambientSendIntervalBase * 10;
      }
      if (batteryVoltage > sleepVoltage && !WiFi.isConnected())
      {
        wifiConnect();
        logWrite("woke up and WiFi reconnected, batteryVoltage: " + String(batteryVoltage), "a");
        ambientSendInterval = ambientSendIntervalBase;
      }
    }
    else
    {
      serialPrintln("myBms.readBmsData() failed");
      logWrite("myBms.readBmsData() failed", "a");
    }
  }
  if (millis() - ambientlLastSent >= ambientSendInterval)
  {
    if (!WiFi.isConnected())
    {
      wifiConnect();
    }
    ambient.set(1, batteryVoltage / 100.0f);
    ambient.set(2, batteryCurrent / 100.0f);
    ambient.set(3, cellDiffVoltage / 1.0f);
    ambient.set(4, (batteryTemp1 + batteryTemp2) / 2 / 10.0f);
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