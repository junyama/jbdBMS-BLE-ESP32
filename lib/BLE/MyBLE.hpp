#ifndef MY_BLE_HPP
#define MY_BLE_HPP

#include "MyDataProcess.hpp"
#include "BLEDevice.h"
#include "MyAdvertisedDeviceCallbacks.hpp"
#include "MyClientCallback.hpp"

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
	BLEUUID charUUID_tx;							// xiaoxiang bms original module //m
	BLEUUID charUUID_rx;							// xiaoxiang bms original module //m

	//BLEAdvertisedDevice *myDevice;

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
	static bool newPacketReceived;

	static packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
	//static packCellInfoStruct packCellInfo; // here shall be the latest data got from BMS

	MyBLE();
	void printBasicInfo(); // debug all data to uart
	void printCellInfo();  // debug all data to uart
	void bleStartup();
	void disconnectFromServer(); // does not work as intended, but automatically reconnected
	void bleRequestData();
};

#endif /* MY_BLE_HPP */