#ifndef MY_AMBIENT2_HPP
#define MY_AMBIENT2_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ambient.h>

#include "MyDebug.hpp"
#include <Arduino.h>
#include "MyBLE2.hpp"

#define MSG_BUFFER_SIZE (50)

class MyAmbient2
{
private:
    const String TAG = "MyAmbient";

    unsigned int channelId = 1234;
    String writeKey = "xxxxxxxxxxxxxx";
    unsigned int ambientSendIntervalBaseMs = 60 * 1000;
    unsigned int ambientSendIntervalMs = 60 * 1000;
    Ambient client;

public:
    unsigned long ambientlLastSent = 0;

    MyAmbient2();
    void begin(JsonDocument configJson, WiFiClient *wifiClient);
    void setLongInterval();
    void resetInterval();
    bool timeout(int currentTime);
    void set(float *values);
    void send();
};

#endif /* MY_AMBIENT2_HPP */