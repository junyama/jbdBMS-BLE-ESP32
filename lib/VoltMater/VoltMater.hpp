#ifndef VOLT_MATER_HPP
#define VOLT_MATER_HPP

#include "M5Core2.h"
#include "MyDebug.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>

#include "M5_ADS1115.h"

#define M5_UNIT_VMETER_I2C_ADDR 0x49
#define M5_UNIT_VMETER_EEPROM_I2C_ADDR 0x53
#define M5_UNIT_VMETER_PRESSURE_COEFFICIENT 0.015918958F

#define MSG_BUFFER_SIZE (50)

class VoltMater
{
private:
    const String TAG = "VoltMater";
    ADS1115 vmeter;
    bool enabled = true;
    float resolution = 0.0;
    float calibration_factor = 0.0;

public:
    float calVoltage;
    int measurmentIntervalMs = 60000;
    unsigned long lastMeasurment = 0;
    bool timeout(int currentTime);
    void setup(JsonDocument configJson);
    JsonDocument getVoltage();
};

#endif /* POWER_SAVING2_HPP */