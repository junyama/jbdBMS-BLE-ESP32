#ifndef MY_ADVERTISE_DEVICE_HPP
#define MY_ADVERTISE_DEVICE_HPP

#include "MyDebug2.hpp"
#include "BLEDevice.h"
#include <Arduino.h>

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ // this is called by some underlying magic
  // Called for each advertising BLE server.
private:
	static const String TAG;
	BLEUUID serviceUUID;
	void onResult(BLEAdvertisedDevice advertisedDevice);

public:
	boolean doConnect;
	boolean doScan;

	BLEAdvertisedDevice *myDevice;
	MyAdvertisedDeviceCallbacks();
	MyAdvertisedDeviceCallbacks(BLEUUID serviceUUID);
};

#endif /* MY_ADVERTISE_DEVICE_HPP */