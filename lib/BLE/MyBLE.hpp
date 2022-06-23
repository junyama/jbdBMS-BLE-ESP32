#ifndef MY_BLE_HPP
#define MY_BLE_HPP

#include <Arduino.h>
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
	int32_t Amps;	// unit 1mA
	int32_t Watts;	// unit 1W
	uint16_t CapacityRemainAh;
	uint8_t CapacityRemainPercent; // unit 1%
	uint32_t CapacityRemainWh;	   // unit Wh
	uint16_t Temp1;				   // unit 0.1C
	uint16_t Temp2;				   // unit 0.1C
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

class MyBLE
{
private:
	static const String TAG;
	static const long interval;
	static unsigned long previousMillis;
	static bool toggle;

	static BLEClient *pClient;
	static void bmsGetInfo3();
	static void bmsGetInfo4();
	static void bmsSetInfo();

	static bool connectToServer();
	static void sendCommand(uint8_t *data, uint32_t dataLen);

	static BLERemoteCharacteristic *pRemoteCharacteristic;
	static BLERemoteService *pRemoteService;
	static BLEUUID serviceUUID;
	static BLEUUID charUUID_tx;
	static BLEUUID charUUID_rx;

	// BLEAdvertisedDevice *myDevice;

	static int16_t two_ints_into16(int highbyte, int lowbyte); // turns two bytes into a single long integer
	static bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen);
	static bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen);
	static bool isPacketValid(byte *packet); // check if packet is valid
	static bool bmsProcessPacket(byte *packet);
	static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
	static bool bleCollectPacket(char *data, uint32_t dataSize); // reconstruct packet from BLE incomming data, called by notifyCallback function

public:
	static MyAdvertisedDeviceCallbacks *myAdvertisedDeviceCallbacks;
	static MyClientCallback *myClientCallback;

	/*
	static const int32_t c_cellNominalVoltage; // mV
	static const uint16_t c_cellAbsMin;
	static const uint16_t c_cellAbsMax;
	static const int32_t c_packMaxWatt;
	static const uint16_t c_cellMaxDisbalance;
	*/

	static bool newPacketReceived;

	static packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
	static packCellInfoStruct packCellInfo;	  // here shall be the latest data got from BMS

	//MyBLE();
	static void printBasicInfo(); // debug all data to uart
	static void printCellInfo();  // debug all data to uart
	static void bleStartup();
	static void disconnectFromServer(); // does not work as intended, but automatically reconnected
	static void bleRequestData();
};

#endif /* MY_BLE_HPP */