#ifndef MY_BLE_HPP
#define MY_BLE_HPP

#include "MyCommon.hpp"
#include "BLEDevice.h"

/*
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ // this is called by some underlying magic
  // Called for each advertising BLE server.
private:
	const String TAG;
	void onResult(BLEAdvertisedDevice advertisedDevice);

public:
	MyAdvertisedDeviceCallbacks();
};

class MyClientCallback : public BLEClientCallbacks
{ // this is called on connect / disconnect by some underlying magic+
private:
	const String TAG;
	void onConnect(BLEClient *pclient);
	void onDisconnect(BLEClient *pclient);

public:
	MyClientCallback();
};
*/

class MyBLE
{
private:
	const String TAG;
	unsigned long previousMillis;
	const long interval;
	bool toggle;
	BLEClient *pClient;
	void bmsGetInfo3();
	void bmsGetInfo4();
	bool connectToServer();
	void sendCommand(uint8_t *data, uint32_t dataLen);

	BLERemoteCharacteristic *pRemoteCharacteristic; // m
	BLERemoteService *pRemoteService;				// m
	BLEUUID charUUID_tx;							// xiaoxiang bms original module //m
	BLEUUID charUUID_rx;							// xiaoxiang bms original module //m

public:
	MyBLE();
	void printBasicInfo(); // debug all data to uart
	void printCellInfo();  // debug all data to uart
	void bleStartup();
	void disconnectFromServer(); // does not work as intended, but automatically reconnected
	void bleRequestData();
};

#endif /* MY_BLE_HPP */