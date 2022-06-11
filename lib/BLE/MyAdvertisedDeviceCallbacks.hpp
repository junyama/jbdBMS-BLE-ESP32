#ifndef MY_ADVERTISE_DEVICE_HPP
#define MY_ADVERTISE_DEVICE_HPP

#include "MyDataProcess.hpp"

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ // this is called by some underlying magic
  // Called for each advertising BLE server.
private:
	static const String TAG;
	void onResult(BLEAdvertisedDevice advertisedDevice);
};

#endif /* MY_ADVERTISE_DEVICE_HPP */