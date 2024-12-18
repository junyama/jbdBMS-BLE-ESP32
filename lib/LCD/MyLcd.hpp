#ifndef MY_LCD_HPP
#define MY_LCD_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <ArduinoJson.h>

#include "MyDebug.hpp"
#include <Arduino.h>

#define MSG_BUFFER_SIZE (50)

class MyLcd
{
private:
	static const String TAG;

public:
	static void setup();
	static void showBatteryInfo(float packVoltage, float current, float cellDiff, float temparature1, float temparature2, int capacityRemain);
	//static void showBatteryInfo(float packVoltage);
};

#endif /* MY_LCD_HPP */