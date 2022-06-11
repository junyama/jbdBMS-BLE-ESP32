#ifndef MY_CLIENT_CALLBACK_CPP
#define MY_CLIENT_CALLBACK_CPP

#include "MyClientCallback.hpp"

const String MyClientCallback::TAG = "MyClientCallback";

void MyClientCallback::onConnect(BLEClient *pclient)
{
}

void MyClientCallback::onDisconnect(BLEClient *pclient)
{
    BLE_client_connected = false;
    LOGD(TAG, "onDisconnect");
    // lcdDisconnect();
}

#endif /* MY_CLIENT_CALLBACK_CPP */