#ifndef MY_ADVERTISE_DEVICE_CPP
#define MY_ADVERTISE_DEVICE_CPP

#include "MyAdvertisedDeviceCallbacks.hpp"

using namespace MyLOG;

const String MyAdvertisedDeviceCallbacks::TAG = "MyAdvertisedDeviceCallbacks";

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks()
{
}

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks(BLEUUID serviceUUID_, JsonDocument *configJson_)
    : serviceUUID(serviceUUID_), doConnect(false), doScan(false), configJson(configJson_)
{
}

bool MyAdvertisedDeviceCallbacks::isAddressInConfigList(BLEAdvertisedDevice advertisedDevice)
{
    LOGD(TAG, "Check BLEAddress(" + String(advertisedDevice.getAddress().toString().c_str()) + ") in config.json.");
    JsonDocument bleConfig = (*configJson)["BLE"];
    // JsonDocument *list = configJson->["BLE"]; // compile error
    if (bleConfig.size())
    {
        for (int i = 0; i < bleConfig.size(); i++)
        {
            std::string mac = bleConfig[i]["mac"];
            if (advertisedDevice.getAddress().equals(BLEAddress(mac)))
            {
                LOGD(TAG, "discoved address equals to config: " + String(mac.c_str()));
                return true;
            }
            else
                LOGD(TAG, "discoved address NOT equals to config: " + String(mac.c_str()));
        }
        LOGD(TAG, "discoved address NOT in config");
        return false;
    }
    else
    {
        LOGD(TAG, "No BLE key in coonfigJson. Any mac is OK.");
        return true;
    }
}

void MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice)
{
    LOGD(TAG, "BLE Advertised Device found: " + String(advertisedDevice.toString().c_str()));
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID) && isAddressInConfigList(advertisedDevice))
    {
        LOGD(TAG, "Found our server");
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;
    } // Found our server
}

#endif /* MY_ADVERTISE_DEVICE_CPP */