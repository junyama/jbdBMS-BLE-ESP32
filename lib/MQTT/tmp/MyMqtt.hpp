#ifndef MY_MQTT_HPP
#define MY_MQTT_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "MyDebug.hpp"
#include <Arduino.h>

#define MSG_BUFFER_SIZE (50)

class MyMqtt
{
private:
	static const String TAG;

public:
	PubSubClient *client;

	MyMqtt(WiFiClient wifiClient, const char *mqtt_server, int port);
	void callback(char *topic, byte *payload, unsigned int length);
	void reConnect();
};

#endif /* MY_MQTT_HPP */