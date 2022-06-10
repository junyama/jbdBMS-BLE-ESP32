#ifndef MY_BLE_HPP
#define MY_BLE_HPP

#include "MyCommon.hpp"
#include "BLEDevice.h"

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