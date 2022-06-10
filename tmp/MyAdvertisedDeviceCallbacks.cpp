#ifndef MY_ADVERTISE_DEVICE_CPP
#define MY_ADVERTISE_DEVICE_CPP

#include "MyBLE.hpp"

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks()
    : TAG("MyAdvertisedDeviceCallbacks")
{
}

void MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice)
{
    LOGD(TAG, "BLE Advertised Device found: " + String(advertisedDevice.toString().c_str()));
    // LOGD(TAG, advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {
        LOGD(TAG, "Found our server");
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;

    } // Found our server
}

#endif /* MY_ADVERTISE_DEVICE_CPP */