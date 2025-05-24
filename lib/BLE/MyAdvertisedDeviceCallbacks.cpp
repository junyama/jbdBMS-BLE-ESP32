#ifndef MY_ADVERTISE_DEVICE_CPP
#define MY_ADVERTISE_DEVICE_CPP

#include "MyAdvertisedDeviceCallbacks.hpp"

using namespace MyLOG;

const String MyAdvertisedDeviceCallbacks::TAG = "MyAdvertisedDeviceCallbacks";

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks()
{
}

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks(BLEUUID serviceUUID_, String mac_)
    : serviceUUID(serviceUUID_), doConnect(false), doScan(false), mac(mac_)
{
}

bool MyAdvertisedDeviceCallbacks::isAddressInDeviceList(BLEAdvertisedDevice advertisedDevice)
{
    std::string macStr = std::string(mac.c_str());
    if (advertisedDevice.getAddress().equals(BLEAddress(macStr)))
    {
        LOGD(TAG, "discoved address equals to config MAC: " + mac);
        return true;
    }
    else
    {
        LOGD(TAG, "discoved address NOT equals to config MAC: " + mac);
        return false;
        //return true; for debugging
    }
}

void MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice)
{
    LOGD(TAG, "BLE Advertised Device found: " + String(advertisedDevice.toString().c_str()));
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {
        LOGD(TAG, "service UUID is correct");
        if (isAddressInDeviceList(advertisedDevice))
        {
            LOGD(TAG, "mac is correct");
            deviceName = String(advertisedDevice.getName().c_str());
            LOGD(TAG, "Found our server: " + deviceName);
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }
        else
            LOGD(TAG, "mac is NOT in the list");
    }
    else
        LOGD(TAG, "service UUID is NOT correct");
}

#endif /* MY_ADVERTISE_DEVICE_CPP */