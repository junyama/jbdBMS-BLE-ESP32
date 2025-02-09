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
    String topic;

    MyBLE2 *myBLE;

public:
    PubSubClient client;

    MyMqtt2(MyBLE2 *myBLE_);
    void setup(String server, int port, String mqttTtopic);
    void callback(char *topic, byte *payload, unsigned int length);
    int reConnect();
    void subscribe(String topic);
    void publish(String topic, String message);
    bool connected();
    void loop();
};

#endif /* MY_MQTT2_HPP */