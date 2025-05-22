#ifndef LIPO_MATER_HPP
#define LIPO_MATER_HPP

#include "M5Core2.h"
#include "MyDebug.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>

#include "M5_ADS1115.h"

#define M5_UNIT_VMETER_I2C_ADDR 0x49
#define M5_UNIT_VMETER_EEPROM_I2C_ADDR 0x53
#define M5_UNIT_VMETER_PRESSURE_COEFFICIENT 0.015918958F

#define MSG_BUFFER_SIZE (50)

class LipoMater
{
private:
    const String TAG = "LipoMater";

public:
    bool enabled = false;
    float voltage;
    float current;
    int measurmentIntervalMs = 60000;
    unsigned long lastMeasurment = 0;
    String topic = "junichiLipo_0/";
    bool timeout(int currentTime);
    void setup(JsonDocument deviceObj);
    JsonDocument getState();
};

#endif /* LIPO_MATER_HPP */