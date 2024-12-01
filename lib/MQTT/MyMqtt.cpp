#ifndef MY_MQTT_CPP
#define MY_MQTT_CPP

#include "MyMqtt.hpp"

using namespace MyLOG;

const String MyMqtt::TAG = "MyMqtt";

void MyMqtt::callback(char *topic, byte *payload, unsigned int length)
{
    M5.Lcd.print("Message arrived [");
    M5.Lcd.print(topic);
    M5.Lcd.print("] ");
    for (int i = 0; i < length; i++)
    {
        M5.Lcd.print((char)payload[i]);
    }
    M5.Lcd.println();
}

void MyMqtt::reConnect(PubSubClient *client)
{
    LOGD(TAG, "reConnect() called");
    while (!client->connected())
    {
        M5.Lcd.print("Attempting MQTT connection...");
        // Create a random client ID.
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.
        LOGD(TAG, "before connect");
        bool isConnected = client->connect(clientId.c_str()); // crash here!!!!!!!!!!!
        LOGD(TAG, "after connect");
        // if (client.connect(clientId.c_str()))
        if (isConnected)
        {
            M5.Lcd.printf("\nSuccess\n");
            // Once connected, publish an announcement to the topic.
            client->publish("junichi_M5Core2", "MQTT reconnected");
            // ... and resubscribe.
            //client.subscribe("M5Stack");
        }
        else
        {
            LOGD(TAG, "5");
            M5.Lcd.print("failed, rc=");
            M5.Lcd.print(client->state());
            M5.Lcd.println("try again in 5 seconds");
            delay(5000);
        }
    }
}

#endif /* MY_MQTT_CPP */