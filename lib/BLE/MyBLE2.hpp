#ifndef MY_BLE2_HPP
#define MY_BLE2_HPP

#include <Arduino.h>
#include <ArduinoJson.h>

#include "MyDebug.hpp"
#include "BLEDevice.h"
#include "MyAdvertisedDeviceCallbacks.hpp"
#include "MyClientCallback.hpp"

typedef struct
{
    byte start;
    byte type;
    byte status;
    byte dataLen;
} bmsPacketHeaderStruct;

typedef struct
{
    uint16_t Volts; // unit 1mV
    int32_t Amps;   // unit 1mA
    int32_t Watts;  // unit 1W
    uint16_t CapacityRemainAh;
    uint8_t CapacityRemainPercent; // unit 1%
    uint32_t CapacityRemainWh;     // unit Wh
    uint16_t Temp1;                // unit 0.1C
    uint16_t Temp2;                // unit 0.1C
    uint16_t BalanceCodeLow;
    uint16_t BalanceCodeHigh;
    uint8_t MosfetStatus;
} packBasicInfoStruct;

typedef struct
{
    uint8_t NumOfCells;
    uint16_t CellVolt[15]; // cell 1 has index 0 :-/
    uint16_t CellMax;
    uint16_t CellMin;
    uint16_t CellDiff; // difference between highest and lowest
    uint16_t CellAvg;
    uint16_t CellMedian;
    uint32_t CellColor[15];
    uint32_t CellColorDisbalance[15]; // green cell == median, red/violet cell => median + c_cellMaxDisbalance
} packCellInfoStruct;

class MyBLE2
{
private:
    const String TAG = "MyBLE";

    const long interval = 2000;
    unsigned long previousMillis = 0;
    bool toggle = false;
    bool toggle2 = true;

    BLEClient *pClient;
    BLERemoteCharacteristic *pRemoteCharacteristic;
    BLERemoteService *pRemoteService;

    BLEUUID serviceUUID = BLEUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
    BLEUUID charUUID_tx = BLEUUID("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
    BLEUUID charUUID_rx = BLEUUID("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

    // packBasicInfoStruct packBasicInfo;
    // packCellInfoStruct packCellInfo;
    //  char *deviceName;
    // String deviceNameStr = "";
    // int numberOfTemperature = 2;

    void bmsGetInfo3();
    void bmsGetInfo4();
    void bmsGetInfo5();

    bool connectToServer();
    void sendCommand(uint8_t *data, uint32_t dataLen);

    // BLEAdvertisedDevice *myDevice;

    int16_t two_ints_into16(int highbyte, int lowbyte); // turns two bytes into a single long integer
    bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen);
    bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen);
    bool processDeviceInfo(byte *data, unsigned int dataLen);
    byte calcChecksum(byte *packet);
    bool isPacketValid(byte *packet); // check if packet is valid
    bool bmsProcessPacket(byte *packet);
    void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
    bool bleCollectPacket(char *data, uint32_t dataSize); // reconstruct packet from BLE incomming data, called by notifyCallback function
    // void bmsDisableDischarge();
    // void bmsEnableDischarge();
    void bmsMosfetCtrl();

    // JsonDocument *configJson;

public:
    JsonDocument *configJson;
    String deviceTopic = "junichiBMS_X/";

    byte ctrlCommand = 0;
    byte commandParam = 0;
    MyAdvertisedDeviceCallbacks *myAdvertisedDeviceCallbacks;
    MyClientCallback *myClientCallback;
    // byte ctrlCommand;
    // byte commandParam;
    /*
    const int32_t c_cellNominalVoltage; // mV
    const uint16_t c_cellAbsMin;
    const uint16_t c_cellAbsMax;
    const int32_t c_packMaxWatt;
    const uint16_t c_cellMaxDisbalance;
    */

    bool newPacketReceived = false;

    packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
    packCellInfoStruct packCellInfo;   // here shall be the latest data got from BMS
    // char *deviceName;
    String deviceNameStr = "";
    int numberOfTemperature = 2;

    // MyBLE();
    MyBLE2();
    //MyBLE2(JsonDocument *configJson_);
    void printBasicInfo(); // debug all data to uart
    void printCellInfo();  // debug all data to uart
    void bleStartup();
    void disconnectFromServer(); // does not work as intended, but automatically reconnected
    void bleRequestData();
    // void bmsDisableCharge();
    // void bmsEnableCharge();
    void mosfetCtrl(int chargeStatus, int dischargeStatus);
    void getDeviceNameLoop();
};

#endif /* MY_BLE2_HPP */