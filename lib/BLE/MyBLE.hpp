#ifndef MY_BLE_HPP
#define MY_BLE_HPP

#include <Arduino.h>
//#include "MyDataProcess.hpp"
#include "MyDebug2.hpp"
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

	unsigned long previousMillis;
	bool toggle;
	BLEClient *pClient;
	void bmsGetInfo3();
	void bmsGetInfo4();
	bool connectToServer();
	void sendCommand(uint8_t *data, uint32_t dataLen);

	BLERemoteCharacteristic *pRemoteCharacteristic; // m
	BLERemoteService *pRemoteService;				// m
	BLEUUID serviceUUID;
	BLEUUID charUUID_tx; // xiaoxiang bms original module //m
	BLEUUID charUUID_rx; // xiaoxiang bms original module //m

	// BLEAdvertisedDevice *myDevice;

	static int16_t two_ints_into16(int highbyte, int lowbyte); // turns two bytes into a single long integer
	static bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen);
	static bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen);
	static bool isPacketValid(byte *packet); // check if packet is valid
	static bool bmsProcessPacket(byte *packet);
	static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
	static bool bleCollectPacket(char *data, uint32_t dataSize); // reconstruct packet from BLE incomming data, called by notifyCallback function

public:
	MyAdvertisedDeviceCallbacks *myAdvertisedDeviceCallbacks;
	MyClientCallback *myClientCallback;

	static const int32_t c_cellNominalVoltage; // mV
	static const uint16_t c_cellAbsMin;
	static const uint16_t c_cellAbsMax;
	static const int32_t c_packMaxWatt;
	static const uint16_t c_cellMaxDisbalance;

	static bool newPacketReceived;

	static packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
	static packCellInfoStruct packCellInfo;	  // here shall be the latest data got from BMS

	MyBLE();
	void printBasicInfo(); // debug all data to uart
	void printCellInfo();  // debug all data to uart
	void bleStartup();
	void disconnectFromServer(); // does not work as intended, but automatically reconnected
	void bleRequestData();
};

#endif /* MY_BLE_HPP */