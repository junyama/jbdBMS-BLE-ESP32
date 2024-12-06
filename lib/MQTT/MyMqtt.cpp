#ifndef MY_MQTT_CPP
#define MY_MQTT_CPP

#include "MyMqtt.hpp"

using namespace MyLOG;

const String MyMqtt::TAG = "MyMqtt";

void MyMqtt::callback(char *topic, byte *payload, unsigned int length)
{
    String logStr = "Message arrived[";
    logStr = logStr + topic;
    logStr = logStr + "] ";
    for (int i = 0; i < length; i++)
    {
        logStr = logStr + (char)payload[i];
    }
    LOGD(TAG, logStr);
    LOGLCD(TAG, logStr);

    /*
    M5.Lcd.print("Message arrived [");
    M5.Lcd.print(topic);
    M5.Lcd.print("] ");
    for (int i = 0; i < length; i++)
    {
        M5.Lcd.print((char)payload[i]);
    }
    M5.Lcd.println();
    */
}

void MyMqtt::reConnect(PubSubClient *client)
{
    LOGD(TAG, "reConnect() called");
    while (!client->connected())
    {
        LOGD(TAG, "Attempting MQTT connection...");
        // Create a random client ID.
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.
        bool isConnected = client->connect(clientId.c_str());
        // if (client.connect(clientId.c_str()))
        if (isConnected)
        {
            LOGD(TAG, "Connected.");
            // Once connected, publish an announcement to the topic.
            client->publish("junichi_M5Core2", "MQTT reconnected");
            // ... and resubscribe.
            client->subscribe("junichi_M5Core2");
        }
        else
        {
            String logStr = "failed, rc = ";
            logStr = logStr + (client->state());
            logStr = " try again in 5 seconds";
            LOGLCD(TAG, logStr);
            delay(5000);
        }
    }
}

#endif /* MY_MQTT_CPP */