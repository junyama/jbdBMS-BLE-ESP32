#ifndef MY_MQTT2_CPP
#define MY_MQTT2_CPP

#include "MyMqtt2.hpp"

using namespace MyLOG;

String MyMqtt2::getState()
{
    String msgStr = "{\"deviceName\": " + myBLE->deviceNameStr;
    msgStr += ", \"batteryVoltage\": " + String(myBLE->packBasicInfo.Volts);
    msgStr += ", \"batteryCurrent\": " + String(myBLE->packBasicInfo.Amps);
    msgStr += ", \"batteryTemp1\": " + String(myBLE->packBasicInfo.Temp1);
    if (configJson["numberOfTemperature"] == 2)
        msgStr += ", \"batteryTemp2\": " + String(myBLE->packBasicInfo.Temp2);
    msgStr += ", \"batteryChargePercentage\": " + String(myBLE->packBasicInfo.CapacityRemainPercent);
    msgStr += ", \"chargeStatus\": " + String(myBLE->packBasicInfo.MosfetStatus & 1);
    msgStr += ", \"dischargeStatus\": " + String((myBLE->packBasicInfo.MosfetStatus & 2) >> 1);
    msgStr += ", \"lipoVoltage\": " + String(M5.Axp.GetBatVoltage());
    msgStr += ", \"lipoCurrent\": " + String(M5.Axp.GetBatCurrent());
    String sleepVoltage = configJson["sleepVoltage"];
    msgStr += ", \"sleepVoltage\": " + sleepVoltage;
    String wakeUpVoltageMv = configJson["wakeUpVoltageMv"];
    msgStr += ", \"wakeUpVoltageMv\": " + wakeUpVoltageMv;
    String deepSleepVoltageMv = configJson["deepSleepVoltageMv"];
    msgStr += ", \"deepSleepVoltageMv\": " + deepSleepVoltageMv;
    String deepSleepTimeSec = configJson["deepSleepTimeSec"];
    msgStr += ", \"deepSleepTimeSec\": " + deepSleepTimeSec;
    int ambientSendIntervalBaseMs = configJson["ambient"]["ambientSendIntervalBaseMs"];
    msgStr += ", \"ambientSendIntervalBaseMs\": " + String(ambientSendIntervalBaseMs);
    msgStr += "}";
    return msgStr;
}

MyMqtt2::MyMqtt2(PubSubClient *client_, MyBLE2 *myBLE_)
{
    client = client_;
    myBLE = myBLE_;
}

void MyMqtt2::setup(JsonDocument configJson_)
{
    LOGD(TAG, "Setting MQTT parameters ..........");
    configJson = configJson_;
    String mqttServerConf = configJson["mqtt"]["server"];
    if (mqttServerConf != "null")
    {
        server = mqttServerConf;
        LOGD(TAG, "MQTT changed server: " + server);
    }
    else
        LOGD(TAG, "MQTT default server: " + server);
    int mqqtPortConf = configJson["mqtt"]["port"];
    if (mqqtPortConf)
    {
        port = mqqtPortConf;
        LOGD(TAG, "MQTT server changed port: " + String(port));
    }
    else
        LOGD(TAG, "MQTT server default port: " + String(port));
    String mqttTopicConf = configJson["mqtt"]["topic"];
    if (mqttTopicConf != "null")
    {
        topic = mqttTopicConf;
        LOGD(TAG, "MQTT changed Topic: " + topic);
    }
    else
        LOGD(TAG, "MQTT default Topic: " + topic);
    String mqttUserConf = configJson["mqtt"]["user"];
    if (mqttUserConf != "null")
    {
        user = mqttUserConf;
        LOGD(TAG, "MQTT changed User: " + user);
    }
    else
        LOGD(TAG, "MQTT default User: " + user);
    String mqttPassConf = configJson["mqtt"]["password"];
    if (mqttPassConf != "null")
    {
        password = mqttPassConf;
        LOGD(TAG, "MQTT changed Pass: " + password);
    }
    else
        LOGD(TAG, "MQTT default Pass: " + password);
    client->setServer(server.c_str(), port);
    client->setCallback([this](char *topic_, byte *payload, unsigned int length)
                        { callback(topic_, payload, length); });
}

void MyMqtt2::callback(char *topic_, byte *payload, unsigned int length)
{
    int chargeStatus = myBLE->packBasicInfo.MosfetStatus & 1;
    int dischargeStatus = (myBLE->packBasicInfo.MosfetStatus & 2) >> 1;

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
        if (!client->connected())
        {
            reConnect();
        }
        LOGD(TAG, "responding to getState!");
        publish(("stat/" + topic + "RESULT").c_str(), getState().c_str());

        return;
    }
    if (String(topic_).equals("cmnd/" + topic + "shutdown"))
    {

        if (!client->connected())
        {
            reConnect();
        }
        LOGD(TAG, "responding to shutdown!");
        publish(("stat/" + topic + "RESULT").c_str(), msgStr.c_str());
        delay(2000);
        int sec;
        if (msgStr == "")
            M5.shutdown();
        else
        {
            sec = msgStr.toInt();
            M5.shutdown(sec);
        }
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
                myBLE->mosfetCtrl(0, dischargeStatus);
                chargeStatus = 0;
                msgStr = "OFF";
            }
            else if (msgStr.equals("1"))
            {
                myBLE->mosfetCtrl(1, dischargeStatus);
                chargeStatus = 1;
                msgStr = "ON";
            }
            else if (msgStr.equals("toggle"))
            {
                myBLE->mosfetCtrl((chargeStatus ^ 1), dischargeStatus);
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
            publish(("stat/" + topic + "CHARGE").c_str(), msgStr.c_str());
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
                myBLE->mosfetCtrl(chargeStatus, 0);
                dischargeStatus = 0;
                msgStr = "OFF";
            }
            else if (msgStr.equals("1"))
            {
                myBLE->mosfetCtrl(chargeStatus, 1);
                dischargeStatus = 1;
                msgStr = "ON";
            }
            else if (msgStr.equals("toggle"))
            {
                myBLE->mosfetCtrl(chargeStatus, (dischargeStatus ^ 1));
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
            publish(("stat/" + topic + "DISCARGE").c_str(), msgStr.c_str());
        }
        msgStr = "{\"chargeStatus\": " + String(chargeStatus) + ", \"didchargeStatus\": " + String(dischargeStatus) + "}";
        publish(("stat/" + topic + "RESULT").c_str(), msgStr.c_str());
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

int MyMqtt2::reConnect()
{
    LOGD(TAG, "reConnect() called");
    int i = 0;
    while (!client->connected())
    {
        LOGD(TAG, "Attempting MQTT connection...");
        // Create a random client ID.
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.
        LOGD(TAG, "1.................................");
        bool isConnected = client->connect(clientId.c_str(), user.c_str(), password.c_str());
        LOGD(TAG, "user: " + user + ", password: " + password);
        LOGD(TAG, "2.................................");
        // if (client->connect(clientId.c_str()))
        if (isConnected)
        {
            LOGD(TAG, "Connected.");
            // Once connected, publish an announcement to the topic.
            String topicStr = "stat/" + topic + "STATE";
            publish(topicStr.c_str(), "MQTT reconnected");
            // ... and resubscribe.
            topicStr = "cmnd/" + topic + "#";
            client->subscribe(topicStr.c_str());
            return 0;
        }
        else
        {
            String logStr = "failed, rc = ";
            logStr = logStr + (client->state());
            logStr = " try again in 5 seconds";
            LOGLCD(TAG, logStr);
            delay(5000);
        }
        if (i > 10)
        {
            LOGD(TAG, "failed to reConnect 10 times.");
            return 1;
        }
        i++;
    }
    return 0;
}

void MyMqtt2::subscribe(String topic)
{
    client->subscribe(topic.c_str());
}

void MyMqtt2::publish(String topic, String message)
{
    LOGD(TAG, "publishing >>>>> topic: " + topic + ", massage: " + message);
    if (!client->connected())
    {
        reConnect();
    }
    client->publish(topic.c_str(), message.c_str());
}

bool MyMqtt2::connected()
{
    return (client->connected());
}

void MyMqtt2::loop()
{
    client->loop();
}

#endif /* MY_MQTT2_CPP */