#ifndef MY_CLIENT_CALLBACK_CPP
#define MY_CLIENT_CALLBACK_CPP

#include "MyClientCallback.hpp"

using namespace MyLOG;

const String MyClientCallback::TAG = "MyClientCallback";

MyClientCallback::MyClientCallback()
: BLE_client_connected(false)
{
}

MyClientCallback::MyClientCallback(boolean *p_BLE_client_connected)
{
}

void MyClientCallback::onConnect(BLEClient *pclient)
{
}

void MyClientCallback::onDisconnect(BLEClient *pclient)
{
    //BLE_client_connected = false;
    BLE_client_connected = false;
    LOGD(TAG, "onDisconnect");
    // lcdDisconnect();
}

#endif /* MY_CLIENT_CALLBACK_CPP */