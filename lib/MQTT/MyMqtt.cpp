#ifndef MY_MQTT_CPP
#define MY_MQTT_CPP

#include "MyMqtt.hpp"

using namespace MyLOG;

const String MyMqtt::TAG = "MyMqtt";
PubSubClient *MyMqtt::client;
String MyMqtt::topic = "junichi/M5Core2";
String MyMqtt::server = "broker.emqx.io";

void MyMqtt::setup(PubSubClient *mqttClient, String mqttServer, int mqttPort, String mqttTopic)
{
    LOGD(TAG, "server: " + server);
    client = mqttClient;
    topic = mqttTopic;
    server = mqttServer;
    client->setServer(server.c_str(), mqttPort);
    // client->setServer(mqttServer.c_str(), mqttPort); //does not work
    client->setCallback(callback);
}

void MyMqtt::callback(char *topic_, byte *payload, unsigned int length)
{
    int chargeStatus = MyBLE::packBasicInfo.MosfetStatus & 1;
    int dischargeStatus = (MyBLE::packBasicInfo.MosfetStatus & 2) >> 1;

    String msgStr = "";
    for (int i = 0; i < length; i++)
    {
        msgStr = msgStr + (char)payload[i];
    }
    String logStr = "Message arrived[";
    logStr = logStr + topic_;
    logStr = logStr + "] ";
    logStr = logStr + msgStr;
    LOGD(TAG, logStr);
    LOGLCD(TAG, logStr);

    if (String(topic_).equals("cmnd/" + topic + "getState"))
    {
        String megStr = "{\"batteryVoltage\": " + String(MyBLE::packBasicInfo.Volts) + ", \"batteryCurrent\": " + String(MyBLE::packBasicInfo.Amps) + ", \"batteryTemp1\": " + String(MyBLE::packBasicInfo.Temp1);
        // if (numberOfTemperature == 2)
        megStr += ", \"batteryTemp2\": " + String(MyBLE::packBasicInfo.Temp2);
        megStr += ", \"batteryChargePercentage\": " + String(MyBLE::packBasicInfo.CapacityRemainPercent);
        megStr += ", \"chargeStatus\": " + String(MyBLE::packBasicInfo.MosfetStatus & 1);
        megStr += ", \"dischargeStatus\": " + String((MyBLE::packBasicInfo.MosfetStatus & 2) >> 1);
        megStr += ", \"lipoVoltage\": " + String(M5.Axp.GetBatVoltage());
        megStr += ", \"lipoCurrent\": " + String(M5.Axp.GetBatCurrent()) + "}";
        if (!client->connected())
        {
            reConnect();
        }
        LOGD(TAG, "responding to getState!");
        client->publish(("stat/" + topic + "RESULT").c_str(), megStr.c_str());
        return;
    }
    if ((String(topic_).equals("cmnd/" + topic + "charge")) || ((String(topic_).equals("cmnd/" + topic + "discharge"))))
    {
        if (String(topic_).equals("cmnd/" + topic + "charge"))
        {
            LOGD(TAG, "charge status: " + String(chargeStatus) + ", discharge status: " + String(dischargeStatus));
            if (msgStr.equals(""))
            {
                if (chargeStatus)
                    msgStr = "ON";
                else
                    msgStr = "OFF";
            }
            else if (msgStr.equals("0"))
            {
                MyBLE::mosfetCtrl(0, dischargeStatus);
                chargeStatus = 0;
                msgStr = "OFF";
            }
            else if (msgStr.equals("1"))
            {
                MyBLE::mosfetCtrl(1, dischargeStatus);
                chargeStatus = 1;
                msgStr = "ON";
            }
            else if (msgStr.equals("toggle"))
            {
                MyBLE::mosfetCtrl((chargeStatus ^ 1), dischargeStatus);
                chargeStatus = chargeStatus ^ 1;
                msgStr = "TOGGLE";
            }
            else
            {
                msgStr = "INVALID";
            }
            if (!client->connected())
            {
                reConnect();
            }
            LOGD(TAG, "responding to charge!");
            client->publish(("stat/" + topic + "CHARGE").c_str(), msgStr.c_str());
        }
        else if (String(topic_).equals("cmnd/" + topic + "discharge"))
        {
            LOGD(TAG, "charge status: " + String(chargeStatus) + ", discharge status: " + String(dischargeStatus));
            if (msgStr.equals(""))
            {
                if (dischargeStatus)
                    msgStr = "ON";
                else
                    msgStr = "OFF";
            }
            else if (msgStr.equals("0"))
            {
                MyBLE::mosfetCtrl(chargeStatus, 0);
                dischargeStatus = 0;
                msgStr = "OFF";
            }
            else if (msgStr.equals("1"))
            {
                MyBLE::mosfetCtrl(chargeStatus, 1);
                dischargeStatus = 1;
                msgStr = "ON";
            }
            else if (msgStr.equals("toggle"))
            {
                MyBLE::mosfetCtrl(chargeStatus, (dischargeStatus ^ 1));
                dischargeStatus = dischargeStatus ^ 1;
                msgStr = "TOGGLE";
            }
            else
            {
                msgStr = "INVALID";
            }
            if (!client->connected())
            {
                reConnect();
            }
            LOGD(TAG, "responding to discharge!");
            client->publish(("stat/" + topic + "DISCARGE").c_str(), msgStr.c_str());
        }
        msgStr = "{\"chargeStatus\": " + String(chargeStatus) + ", \"didchargeStatus\": " + String(dischargeStatus) + "}";
        client->publish(("stat/" + topic + "RESULT").c_str(), msgStr.c_str());
        return;
    }
    JsonDocument megJson;
    DeserializationError error = deserializeJson(megJson, msgStr.c_str());
    if (error)
    {
        LOGD(TAG, "Deserialization error: " + msgStr);
        return;
    }
    // msgJson process here
}

void MyMqtt::reConnect()
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
            String topicStr = "stat/" + topic + "STATE";
            client->publish(topicStr.c_str(), "MQTT reconnected");
            // ... and resubscribe.
            topicStr = "cmnd/" + topic + "#";
            client->subscribe(topicStr.c_str());
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

void MyMqtt::subscribe(String topic)
{
    client->subscribe(topic.c_str());
}

void MyMqtt::publish(String topic, String message)
{
    client->publish(topic.c_str(), message.c_str());
}

bool MyMqtt::connected()
{
    return (client->connected());
}

void MyMqtt::loop()
{
    client->loop();
}

#endif /* MY_MQTT_CPP */