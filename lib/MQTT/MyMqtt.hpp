#ifndef MY_MQTT_HPP
#define MY_MQTT_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoJson.h>

#include "MyDebug.hpp"
#include <Arduino.h>
#include "MyBLE.hpp"

#define MSG_BUFFER_SIZE (50)

class MyMqtt
{
private:
	static const String TAG;
	static String topic;

public:
	static PubSubClient *client;
	static String server;
	static void setup(PubSubClient *mqqtClient, String server, int port, String mqttTtopic);
	static void callback(char *topic, byte *payload, unsigned int length);
	static void reConnect();
	static void subscribe(String topic);
	static void publish(String topic, String message);
	static bool connected();
	static void loop();
};

#endif /* MY_MQTT_HPP */