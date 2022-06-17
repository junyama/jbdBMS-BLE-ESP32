#ifndef MY_ADVERTISE_DEVICE_CPP
#define MY_ADVERTISE_DEVICE_CPP

#include "MyAdvertisedDeviceCallbacks.hpp"

using namespace MyLOG;

const String MyAdvertisedDeviceCallbacks::TAG = "MyAdvertisedDeviceCallbacks";

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks()
{
}

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks(BLEUUID serviceUUID_)
    : serviceUUID(serviceUUID_), doConnect(false), doScan(false)
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