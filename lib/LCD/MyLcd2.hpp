#ifndef MY_LCD2_HPP
#define MY_LCD2_HPP

#include "M5Core2.h"
#include <WiFi.h>
#include <ArduinoJson.h>

#include "MyDebug.hpp"
#include <Arduino.h>

#define MSG_BUFFER_SIZE (50)

class MyLcd2
{
private:
	 const String TAG = "MyLcd";
	 bool isBatteryInfoShown = false;

public:
	 void setup();
	 void println(String text);
	 void showBatteryInfo(float packVoltage, float current, float cellDiff, float temparature1, float temparature2, int capacityRemain);
};

#endif /* MY_LCD_HPP */