#ifndef MY_MQTT2_HPP
#define MY_MQTT2_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "MyDebug.hpp"
#include <Arduino.h>
#include "MyBLE2.hpp"

#define MSG_BUFFER_SIZE (50)

class MyMqtt2
{
private:
    const String TAG = "MyMqtt2";
    String server = "broker.emqx.io";
    int port = 1883;
    String user = "mqtt-user";
    String password = "mqttpass";
    int messageSizeLimit = 128;
    JsonDocument configJson;
    MyBLE2 *myBLE;

public:
    PubSubClient *client;

    String topic = "junichi/M5Core2/";

    MyMqtt2(PubSubClient *client_, MyBLE2 *myBLE_);
    void setup(JsonDocument configJson);
    void callback(char *topic, byte *payload, unsigned int length);
    int reConnect();
    void subscribe(String topic);
    void publish(String topic, String message);
    bool connected();
    void loop();
    String getState();
    String getLipoState();
    String getConfiguration();
};

#endif /* MY_MQTT2_HPP */