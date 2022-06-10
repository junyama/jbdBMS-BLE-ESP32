#ifndef MY_ADVERTISE_DEVICE_HPP
#define MY_ADVERTISE_DEVICE_HPP

#include "MyCommon.hpp"

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ // this is called by some underlying magic
  // Called for each advertising BLE server.
private:
	const String TAG;
	void onResult(BLEAdvertisedDevice advertisedDevice);

public:
	MyAdvertisedDeviceCallbacks();
};

#endif /* MY_ADVERTISE_DEVICE_HPP */