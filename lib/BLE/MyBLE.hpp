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

public:
	MyAdvertisedDeviceCallbacks *myAdvertisedDeviceCallbacks;
	MyClientCallback *myClientCallback; 

	//packBasicInfoStruct packBasicInfo2; // here shall be the latest data got from BMS
	//packCellInfoStruct packCellInfo2; // here shall be the latest data got from BMS

	MyBLE();
	void printBasicInfo(); // debug all data to uart
	void printCellInfo();  // debug all data to uart
	void bleStartup();
	void disconnectFromServer(); // does not work as intended, but automatically reconnected
	void bleRequestData();
};

#endif /* MY_BLE_HPP */