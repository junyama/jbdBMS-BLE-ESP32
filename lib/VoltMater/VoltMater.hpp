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
    float resolution = 0.0;
    float calibration_factor = 0.0;

public:
    bool available = false;
    float calVoltage;
    int measurmentIntervalMs = 10000;
    unsigned long lastMeasurment = 0;
    String topic = "junichiVoltMater_0/";
    bool timeout(int currentTime);
    void setup(JsonDocument deviceObj);
    JsonDocument getState();
};

#endif /* POWER_SAVING2_HPP */