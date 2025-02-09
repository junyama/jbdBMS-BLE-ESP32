#ifndef MY_AMBIENT_HPP
#define MY_AMBIENT_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ambient.h>

#include "MyDebug.hpp"
#include <Arduino.h>
#include "MyBLE.hpp"

#define MSG_BUFFER_SIZE (50)

class MyAmbient
{
private:
    static const String TAG;

    static unsigned int channelId;
    static String writeKey;
    static unsigned long ambientlLastSent;
    static unsigned int ambientSendIntervalBaseMs; // milli sec
    static unsigned int ambientSendIntervalMs;
    static Ambient *client;

public:
    static void setup(Ambient *ambientClient, JsonDocument configJson, WiFiClient *wifiClient);
    static void setLongInterval();
    static void set(float *values);
    static void send();
};

#endif /* MY_AMBIENT_HPP */