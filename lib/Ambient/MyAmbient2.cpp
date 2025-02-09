#ifndef MY_AMBIENT_CPP
#define MY_AMBIENT_CPP

#include "MyAmbient2.hpp"

using namespace MyLOG;

MyAmbient2::MyAmbient2()
{
}

void MyAmbient2::begin(JsonDocument configJson, WiFiClient *wifiClient)
{
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

    client.begin(channelId, writeKey.c_str(), wifiClient);
    LOGD(TAG, "ambientlLastSent initial value: " + String(ambientlLastSent));
    LOGD(TAG, "ambient setup done");
}

void MyAmbient2::setLongInterval()
{
    ambientSendIntervalMs = ambientSendIntervalBaseMs * 10;
}

void MyAmbient2::resetInterval()
{
    ambientSendIntervalMs = ambientSendIntervalBaseMs;
}

bool MyAmbient2::timeout(int currentTime)
{
    if ((currentTime - ambientlLastSent) >= ambientSendIntervalMs)
    {
        LOGD(TAG, "millis() - ambientlLastSent: " + String(currentTime) + " - " + String(ambientlLastSent) + " >= ambientSendIntervalMs: " + String(ambientSendIntervalMs));
        return true;
    }
    else
        return false;
}

void MyAmbient2::set(float *values)
{
    String logStr = "values: {";
    for (int i = 0; i < sizeof(values); i++)
    {
        client.set(i + 1, values[i]);
        logStr += String(values[i]) + ", ";
    }
    LOGD(TAG, logStr + "}");
}

void MyAmbient2::send()
{
    client.send();
    LOGD(TAG, "ambient sent to channelId: " + String(channelId));
}

#endif /* MY_AMBIENT2_CPP */