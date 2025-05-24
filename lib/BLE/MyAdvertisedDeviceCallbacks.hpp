#ifndef MY_ADVERTISE_DEVICE_HPP
#define MY_ADVERTISE_DEVICE_HPP

#include "MyDebug.hpp"
#include "BLEDevice.h"
#include <Arduino.h>
#include <ArduinoJson.h>

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ // this is called by some underlying magic
  // Called for each advertising BLE server.
private:
	static const String TAG;
	BLEUUID serviceUUID;
	//JsonDocument *configJson;
	String mac;
	void onResult(BLEAdvertisedDevice advertisedDevice);
	//bool isAddressInConfigList(BLEAdvertisedDevice advertisedDevice);
	bool isAddressInDeviceList(BLEAdvertisedDevice advertisedDevice);


public:
	boolean doConnect;
	boolean doScan;

	BLEAdvertisedDevice *myDevice;
	String deviceName = "NOT_FOUND";
	MyAdvertisedDeviceCallbacks();
	MyAdvertisedDeviceCallbacks(BLEUUID serviceUUID, String mac_);
};

#endif /* MY_ADVERTISE_DEVICE_HPP */