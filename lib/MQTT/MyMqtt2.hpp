#ifndef MY_MQTT2_HPP
#define MY_MQTT2_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#include "MyDebug.hpp"
#include <Arduino.h>
#include "MyBLE2.hpp"
#include "VoltMater.hpp"

#define MSG_BUFFER_SIZE (50)

class MyMqtt2
{
private:
    const String TAG = "MyMqtt2";
    bool disabled = false;
    String server = "junichi.ddns.net";
    int port = 1883;
    String user = "mqtt-user";
    String password = "mqttpass";
    int messageSizeLimit = 128;
    JsonDocument configJson;
    MyBLE2 *myBLE;
    VoltMater *voltMater;
    void reset();

public:
    PubSubClient *client;

    String topic = "junichi/M5Core2/";

    MyMqtt2();
    MyMqtt2(PubSubClient *client_, WiFiClient wifiClient, MyBLE2 *myBLE_, VoltMater *voltMater_);
    void setup(WiFiClient wifiClient, JsonDocument configJson);
    void callback(char *topic, byte *payload, unsigned int length);
    void reConnect();
    void subscribe(String topic);
    void publish(String topic, String message);
    void publishJson(String topic, JsonDocument json, bool retain);
    bool connected();
    void loop();
    String getState();
    JsonDocument getState2();
    String getLipoState();
    String getConfiguration();
    JsonDocument getBmsState();
    void publishHaDiscovery();
};

#endif /* MY_MQTT2_HPP */