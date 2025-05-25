#ifndef VOLT_MATER_CPP
#define VOLT_MATER_CPP

#include "VoltMater.hpp"

using namespace MyLOG;

void VoltMater::setup(JsonDocument deviceObj)
{
    available = true;
    int i = 1;
    while (!vmeter.begin(&Wire, M5_UNIT_VMETER_I2C_ADDR, 32, 33, 400000U))
    {
        LOGD(TAG, String(i) + ": Unit vmeter Init Fail");
        //M5.Lcd.println("Unit vmeter Init Fail");
        if (i > 2)
        {
            LOGD(TAG, "gave up using volt mater.");
            M5.Lcd.println("gave up using volt mater.");
            available = false;
            return;
        }
        i++;
        delay(1000);
    }
    vmeter.setEEPROMAddr(M5_UNIT_VMETER_EEPROM_I2C_ADDR);
    vmeter.setMode(ADS1115_MODE_SINGLESHOT);
    vmeter.setRate(ADS1115_RATE_8);
    vmeter.setGain(ADS1115_PGA_512);
    // | PGA      | Max Input Voltage(V) |
    // | PGA_6144 |        128           |
    // | PGA_4096 |        64            |
    // | PGA_2048 |        32            |
    // | PGA_512  |        16            |
    // | PGA_256  |        8             |

    if (deviceObj["resolution"])
        resolution = deviceObj["resolution"];
    else
        resolution = vmeter.getCoefficient() / M5_UNIT_VMETER_PRESSURE_COEFFICIENT;

    if (deviceObj["calibration_factor"])
        calibration_factor = deviceObj["calibration_factor"];
    else
        calibration_factor = vmeter.getFactoryCalibration();
    if (deviceObj["measurmentIntervalMs"])
        measurmentIntervalMs = deviceObj["measurmentIntervalMs"];
}

bool VoltMater::timeout(int currentTime)
{
    if ((currentTime - lastMeasurment) >= measurmentIntervalMs)
    {
        LOGD(TAG, "millis() - lastMeasument: " + String(currentTime) + " - " + String(lastMeasurment) + " >= measurmentIntervalMs: " + String(measurmentIntervalMs));
        return true;
    }
    else
        return false;
}

JsonDocument VoltMater::getState()
{
    JsonDocument doc;
    if (available)
    {
        int16_t adc_raw = vmeter.getSingleConversion();
        float voltage = adc_raw * resolution * calibration_factor;
        doc["calADC"] = adc_raw * calibration_factor;
        calVoltage = floor(voltage / 10) / 100;
        doc["calVoltage"] = calVoltage;
        doc["rawADC"] = adc_raw;
    } else {
        doc["calADC"] = 0.0;
        doc["calVoltage"] = 0.0;
        calVoltage = 0.0;
        doc["rawADC"] = 0;
    }
    return doc;
}

#endif /* VOLT_MATER_CPP */