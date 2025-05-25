#ifndef VOLT_MATER_CPP
#define VOLT_MATER_CPP

#include "LipoMater.hpp"

using namespace MyLOG;

void LipoMater::setup(JsonDocument deviceObj)
{
    available = true;
    if (deviceObj["measurmentIntervalMs"])
        measurmentIntervalMs = deviceObj["measurmentIntervalMs"];
    String topic_ = deviceObj["mqtt"]["topic"];
    LOGD(TAG, "deviceObj[\"topic\"]: " + topic_);
    if (topic_ != "null")
        topic = topic_;
}

bool LipoMater::timeout(int currentTime)
{
    if ((currentTime - lastMeasurment) >= measurmentIntervalMs)
    {
        LOGD(TAG, "millis() - lastMeasument: " + String(currentTime) + " - " + String(lastMeasurment) + " >= measurmentIntervalMs: " + String(measurmentIntervalMs));
        return true;
    }
    else
        return false;
}

JsonDocument LipoMater::getState()
{
    JsonDocument doc;
    if (available)
    {
        voltage = M5.Axp.GetBatVoltage();
        doc["voltage"] = voltage;
        current = M5.Axp.GetBatCurrent();
        doc["current"] = current;
    }
    else
    {
        doc["voltage"] = 0;
        doc["current"] = 0;
    }
    return doc;
}

#endif /* LIPO_MATER_CPP */