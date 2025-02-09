#ifndef MY_AMBIENT_CPP
#define MY_AMBIENT_CPP

#include "MyAmbient.hpp"

using namespace MyLOG;

const String MyAmbient::TAG = "MyAmbient";
unsigned int MyAmbient::channelId = 1234;
String MyAmbient::writeKey = "xxxxxxxxxxxxxx";
unsigned long MyAmbient::ambientlLastSent = 0;
unsigned int MyAmbient::ambientSendIntervalBaseMs = 60 * 1000; // milli sec
unsigned int MyAmbient::ambientSendIntervalMs = ambientSendIntervalBaseMs;
Ambient *MyAmbient::client = nullptr;

void MyAmbient::setup(Ambient *ambientClient, JsonDocument configJson, WiFiClient *wifiClient)
{
    client = ambientClient;
    int channelId_ = configJson["ambient"]["channelId"];
    if (channelId_)
        channelId = channelId_;
    const char *writeKey_ = configJson["ambient"]["writeKey"];
    if (*writeKey_)
    {
        writeKey = writeKey_;
        LOGD(TAG, "writeKey: " + writeKey);
    }
    int ambientSendIntervalBaseMs_ = configJson["ambient"]["ambientSendIntervalBaseMs"];
    if (ambientSendIntervalBaseMs_)
    {
        ambientSendIntervalBaseMs = ambientSendIntervalBaseMs_;
        ambientSendIntervalMs = ambientSendIntervalBaseMs;
    }

    client->begin(channelId, writeKey.c_str(), wifiClient);
}

void MyAmbient::setLongInterval()
{
    ambientSendIntervalMs = ambientSendIntervalBaseMs * 10;
}

void MyAmbient::set(float *values)
{
    for (int i = 0; i < sizeof(values); i++)
    {
        client->set(i + 1, values[i]);
    }
}

void MyAmbient::send()
{
    client->send();
}

#endif /* MY_AMBIENT_CPP */