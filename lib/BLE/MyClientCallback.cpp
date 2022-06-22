#ifndef MY_CLIENT_CALLBACK_CPP
#define MY_CLIENT_CALLBACK_CPP

#define BLE_LED 33

#include "MyClientCallback.hpp"

using namespace MyLOG;

const String MyClientCallback::TAG = "MyClientCallback";

MyClientCallback::MyClientCallback()
: BLE_client_connected(false)
{
    pinMode(BLE_LED, OUTPUT);
    digitalWrite(BLE_LED, LOW);
}

MyClientCallback::MyClientCallback(boolean *p_BLE_client_connected)
{
}

void MyClientCallback::onConnect(BLEClient *pclient)
{
    digitalWrite(BLE_LED, HIGH);
    LOGD(TAG, "onConnect");
}

void MyClientCallback::onDisconnect(BLEClient *pclient)
{
    //BLE_client_connected = false;
    BLE_client_connected = false;
    digitalWrite(BLE_LED, LOW);
    LOGD(TAG, "onDisconnect");
    // lcdDisconnect();
}

#endif /* MY_CLIENT_CALLBACK_CPP */