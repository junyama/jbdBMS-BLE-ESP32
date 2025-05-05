#ifndef MY_MQTT2_CPP
#define MY_MQTT2_CPP

#include "MyMqtt2.hpp"

using namespace MyLOG;

/*
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
    msgStr += "}";
    return msgStr;
}
*/

JsonDocument MyMqtt2::getState(int deviceId)
{
    JsonDocument doc;
    doc["deviceName"] = myBleArr[deviceId].deviceNameStr;
    doc["batteryVoltage"] = String(myBleArr[deviceId].packBasicInfo.Volts / 1000.0);
    doc["batteryCurrent"] = String(myBleArr[deviceId].packBasicInfo.Amps / 1000.0);
    doc["batteryTemp1"] = String(myBleArr[deviceId].packBasicInfo.Temp1 / 10.0);
    if (configJson["numberOfTemperature"] == 2)
        doc["batteryTemp2"] = String(myBleArr[deviceId].packBasicInfo.Temp2 / 10.0);
    doc["batteryChargePercentage"] = String(myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
    doc["chargeStatus"] = String(myBleArr[deviceId].packBasicInfo.MosfetStatus & 1);
    doc["dischargeStatus"] = String((myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1);
    JsonDocument doc2 = voltMater->getVoltage();
    doc["calVoltage"] = doc2["calVoltage"];
    doc["lipoVoltage"] = String(M5.Axp.GetBatVoltage());
    doc["lipoCurrent"] = String(M5.Axp.GetBatCurrent());
    return doc;
}

/*
JsonDocument MyMqtt2::getState2()
{
    JsonDocument doc;
    doc["deviceName"] = myBLE->deviceNameStr;
    doc["batteryVoltage"] = String(myBLE->packBasicInfo.Volts / 1000.0);
    doc["batteryCurrent"] = String(myBLE->packBasicInfo.Amps / 1000.0);
    doc["batteryTemp1"] = String(myBLE->packBasicInfo.Temp1 / 10.0);
    if (configJson["numberOfTemperature"] == 2)
        doc["batteryTemp2"] = String(myBLE->packBasicInfo.Temp2 / 10.0);
    doc["batteryChargePercentage"] = String(myBLE->packBasicInfo.CapacityRemainPercent);
    doc["chargeStatus"] = String(myBLE->packBasicInfo.MosfetStatus & 1);
    doc["dischargeStatus"] = String((myBLE->packBasicInfo.MosfetStatus & 2) >> 1);
    JsonDocument doc2 = voltMater->getVoltage();
    doc["calVoltage"] = doc2["calVoltage"];
    doc["lipoVoltage"] = String(M5.Axp.GetBatVoltage());
    doc["lipoCurrent"] = String(M5.Axp.GetBatCurrent());
    return doc;
}
*/

JsonDocument MyMqtt2::getBmsState(int deviceId)
{
    JsonDocument doc;
    doc["deviceName"] = myBleArr[deviceId].deviceNameStr;
    doc["batteryVoltage"] = String(myBleArr[deviceId].packBasicInfo.Volts / 1000.0);
    doc["batteryCurrent"] = String(myBleArr[deviceId].packBasicInfo.Amps / 1000.0);
    doc["batteryTemp1"] = String(myBleArr[deviceId].packBasicInfo.Temp1 / 10.0);
    if (configJson["numberOfTemperature"] == 2)
        doc["batteryTemp2"] = String(myBleArr[deviceId].packBasicInfo.Temp2 / 10.0);
    doc["batteryChargePercentage"] = String(myBleArr[deviceId].packBasicInfo.CapacityRemainPercent);
    doc["chargeStatus"] = String(myBleArr[deviceId].packBasicInfo.MosfetStatus & 1);
    doc["dischargeStatus"] = String((myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1);
    JsonDocument doc2 = voltMater->getVoltage();
    doc["calVoltage"] = doc2["calVoltage"];
    doc["lipoVoltage"] = String(M5.Axp.GetBatVoltage());
    doc["lipoCurrent"] = String(M5.Axp.GetBatCurrent());
    doc["sleepVoltage"] = configJson["sleepVoltage"];
    doc["wakeUpVoltageMv"] = configJson["wakeUpVoltageMv"];
    doc["deepSleepVoltageMv"] = configJson["deepSleepVoltageMv"];
    doc["deepSleepTimeSec"] = configJson["deepSleepTimeSec"];
    doc["ambientSendIntervalBaseMs"] = configJson["ambient"]["ambientSendIntervalBaseMs"];
    return doc;
}

void MyMqtt2::publishHaDiscovery()
{
    String discoveryTopic;
    JsonDocument discoveryPayload;
    JsonArray deviceList = configJson["devices"].as<JsonArray>();

    LOGD(TAG, "loading discovery payload from config.json");
    for (int deviceId = 0; deviceId < numberOfDevices; deviceId++)
    {
        JsonDocument deviceObj = deviceList[deviceId];
        String topic = deviceObj["mqtt"]["topic"];
        discoveryTopic = "homeassistant/device/" + topic + "config";
        discoveryPayload = deviceObj["mqtt"]["discoveryPayload"];
        LOGD(TAG, "publishing for HA discovery......");
        publishJson(discoveryTopic, discoveryPayload, true);
        LOGD(TAG, "setting myBleArr[" + String(deviceId) + "].deviceTopic " + topic);
        myBleArr[deviceId].deviceTopic = topic;
    }
}

MyMqtt2::MyMqtt2()
{
}

void MyMqtt2::setup(WiFiClient *wifiClient, MyBLE2 *myBLE_, MyBLE2 *myBleArr_, VoltMater *voltMater_, JsonDocument configJson_)
{
    LOGD(TAG, "Setting MQTT parameters ..........");
    client = new PubSubClient(*wifiClient);
    //myBLE = myBLE_;
    myBleArr = myBleArr_;
    voltMater = voltMater_;
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
        hostTopic = mqttTopicConf;
        LOGD(TAG, "MQTT changed hostTopic: " + hostTopic);
    }
    else
        LOGD(TAG, "MQTT default Topic: " + hostTopic);
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
    /*
    int messageSizeLimitConf = configJson["mqtt"]["messageSizeLimit"];
    if (messageSizeLimitConf)
    {
        messageSizeLimit = messageSizeLimitConf;
        LOGD(TAG, "MQTT changed messageSizeLimit: " + String(messageSizeLimit));
    }
    else
        LOGD(TAG, "MQTT default messageSizeLimit: " + String(messageSizeLimit));
    */
    client->setServer(server.c_str(), port);
    client->setCallback([this](char *topic_, byte *payload, unsigned int length)
                        { callback(topic_, payload, length); });
}

void MyMqtt2::callback(char *topic_, byte *payload, unsigned int length)
{
    int chargeStatus;
    int dischargeStatus;

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

    String deviceTopic;
    for (int deviceId = 0; deviceId < numberOfDevices; deviceId++)
    {
        chargeStatus = myBleArr[deviceId].packBasicInfo.MosfetStatus & 1;
        dischargeStatus = (myBleArr[deviceId].packBasicInfo.MosfetStatus & 2) >> 1;
        deviceTopic = myBleArr[deviceId].deviceTopic;
        LOGD(TAG, "myBleArr[" + String(deviceId) + "].deviceTopic: " + myBleArr[deviceId].deviceTopic);
        if (String(topic_).equals("cmnd/" + deviceTopic + "getState"))
        {
            LOGD(TAG, "responding to getState!");
            publishJson(("stat/" + deviceTopic + "RESULT").c_str(), getState(deviceId), false);
            return;
        }
        if (String(topic_).equals("cmnd/" + deviceTopic + "getBmsState"))
        {
            LOGD(TAG, "responding to getBmsState!");
            publishJson("stat/" + deviceTopic + "RESULT", getBmsState(deviceId), false);
            publishJson("stat/" + deviceTopic + "STATE", getBmsState(deviceId), false);
            return;
        }
        if (String(topic_).equals("cmnd/" + deviceTopic + "getState"))
        {
            LOGD(TAG, "responding to getState!");
            publishJson(("stat/" + deviceTopic + "RESULT").c_str(), getState(deviceId), false);
            return;
        }
        if ((String(topic_).equals("cmnd/" + deviceTopic + "charge")) || ((String(topic_).equals("cmnd/" + deviceTopic + "discharge"))))
        {
            if (String(topic_).equals("cmnd/" + deviceTopic + "charge"))
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
                    myBleArr[deviceId].mosfetCtrl(0, dischargeStatus);
                    chargeStatus = 0;
                    msgStr = "OFF";
                }
                else if (msgStr.equals("1"))
                {
                    myBleArr[deviceId].mosfetCtrl(1, dischargeStatus);
                    chargeStatus = 1;
                    msgStr = "ON";
                }
                else if (msgStr.equals("toggle"))
                {
                    myBleArr[deviceId].mosfetCtrl((chargeStatus ^ 1), dischargeStatus);
                    chargeStatus = chargeStatus ^ 1;
                    msgStr = "TOGGLE";
                }
                else
                {
                    msgStr = "INVALID";
                }
                LOGD(TAG, "responding to charge!");
                publish(("stat/" + deviceTopic + "CHARGE").c_str(), msgStr.c_str());
            }
            else if (String(topic_).equals("cmnd/" + deviceTopic + "discharge"))
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
                    myBleArr[deviceId].mosfetCtrl(chargeStatus, 0);
                    dischargeStatus = 0;
                    msgStr = "OFF";
                }
                else if (msgStr.equals("1"))
                {
                    myBleArr[deviceId].mosfetCtrl(chargeStatus, 1);
                    dischargeStatus = 1;
                    msgStr = "ON";
                }
                else if (msgStr.equals("toggle"))
                {
                    myBleArr[deviceId].mosfetCtrl(chargeStatus, (dischargeStatus ^ 1));
                    dischargeStatus = dischargeStatus ^ 1;
                    msgStr = "TOGGLE";
                }
                else
                {
                    msgStr = "INVALID";
                }
                LOGD(TAG, "responding to discharge!");
                publish(("stat/" + deviceTopic + "DISCARGE").c_str(), msgStr.c_str());
            }
            msgStr = "{\"chargeStatus\": " + String(chargeStatus) + ", \"dischargeStatus\": " + String(dischargeStatus) + "}";
            publish(("stat/" + deviceTopic + "RESULT").c_str(), msgStr.c_str());
            publish(("stat/" + deviceTopic + "STATE").c_str(), msgStr.c_str());
            return;
        }
    }

    ///////////////

    if (String(topic_).equals("cmnd/" + hostTopic + "shutdown"))
    {
        LOGD(TAG, "responding to shutdown!");
        publish(("stat/" + hostTopic + "RESULT").c_str(), msgStr.c_str());
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
    JsonDocument megJson;
    DeserializationError error = deserializeJson(megJson, msgStr.c_str());
    if (error)
    {
        LOGD(TAG, "Deserialization error: " + msgStr);
        return;
    }
    // msgJson process here
}

void MyMqtt2::reConnect()
{
    LOGD(TAG, "reConnect() called");
    int i = 1;
    while (!client->connected())
    {
        LOGD(TAG, "Attempting MQTT connection...");
        // Create a random client ID.
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.
        bool isConnected = client->connect(clientId.c_str(), user.c_str(), password.c_str());
        LOGD(TAG, "user: " + user + ", password: " + password);
        // if (client->connect(clientId.c_str()))
        if (isConnected)
        {
            LOGD(TAG, "Connected.");
            // Once connected, publish an announcement to the topic.
            String topicStr = "stat/" + hostTopic + "STATE";
            publish(topicStr.c_str(), "MQTT reconnected");
            // ... and resubscribe.
            topicStr = "cmnd/" + hostTopic + "#";
            client->subscribe(topicStr.c_str());
            // Home aAssistant discoverry
            // publishHaDiscovery(); //this makes reconnect fail loop
            return;
        }
        else
        {
            String logStr = String(i) + ": ";
            logStr += "failed reconnecting, rc = ";
            logStr += client->state();
            LOGD(TAG, logStr);
            if (i == 5)
            {
                LOGD(TAG, "failed to reConnect many times.");
                disabled = true;
                reset();
            }
            if (i == 3)
            {
                LOGD(TAG, "failed to reConnect a few times. Use the second server");
                String mqttServerConf2 = configJson["mqtt"]["server2"];
                if (mqttServerConf2 != "null")
                {
                    server = mqttServerConf2;
                    LOGD(TAG, "MQTT changed server: " + server);
                }
                client->setServer(server.c_str(), port);
            }
            i++;
            LOGD(TAG, "try to reconnect again in 1 seconds");
            delay(1000);
        }
    }
    return;
}

void MyMqtt2::subscribe(String topic)
{
    if (disabled)
        return;
    client->subscribe(topic.c_str());
}

void MyMqtt2::publish(String topic, String message)
{
    if (disabled)
        return;
    LOGD(TAG, "publishing >>>>> topic: " + topic + ", message: " + message);
    if (!client->connected())
    {
        reConnect();
    }
    for (int i = 0; i < message.length(); i = i + messageSizeLimit)
    {
        client->publish(topic.c_str(), message.substring(i, i + messageSizeLimit).c_str());
    }
}

void MyMqtt2::publishJson(String topic, JsonDocument doc, bool retained)
{
    if (disabled)
        return;
    String jsonStr;
    serializeJson(doc, jsonStr);
    LOGD(TAG, "publishing Json >>>>> topic: " + topic + ", payload: " + jsonStr);
    if (!client->connected())
    {
        reConnect();
    }
    /*
    client->beginPublish(topic.c_str(), measureJson(doc), retained);
    serializeJson(doc, *client);
    client->endPublish();
    */
    client->beginPublish(topic.c_str(), measureJson(doc), retained);
    BufferingPrint bufferedClient(*client, 32);
    serializeJson(doc, bufferedClient);
    bufferedClient.flush();
    client->endPublish();
}

bool MyMqtt2::connected()
{
    return (client->connected());
}

void MyMqtt2::loop()
{
    if (!client->connected())
    {
        reConnect();
    }
    client->loop();
}

void MyMqtt2::reset()
{
    LOGD(TAG, "going to reset in 5 sec");
    delay(5000);
    // ESP.restart();
    M5.shutdown(10);
}

#endif /* MY_MQTT2_CPP */