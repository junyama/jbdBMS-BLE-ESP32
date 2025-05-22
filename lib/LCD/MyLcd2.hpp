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
	int height;
	int width;
	bool isBatteryInfoShown = false;
	typedef struct
	{
		String deviceName = "== UNKOWN ==";
		float volt = 0.0;	 // unit 1mV
		float current = 0.0; // unit 1mA
		float cellDiff = 0.0;
		int capacityRemain = 0;	  // unit 1%
		float temparature1 = 0.0; // unit 0.1C
		float temparature2 = 0.0;
		// float lipoVolt;
		// float lipoCurrent;
	} BmsInfoStruct;

	typedef struct
	{
		float vMater;
		float lipoVolt;
		float lipoCurrent;
	} VMaterLipoInfoStruct;
	VMaterLipoInfoStruct vMaterLipoInfo;

public:
	int bmsIndexShown = 1;
	BmsInfoStruct bmsInfoArr[3];

	void setup();
	void println(String text);
	// void loop();
	void updateBmsInfo(int bmsIndex, float packVoltage, float current, float cellDiff, float temparature, float mainVolt, int capacityRemain);
	void updateVoltMaterInfo(float volt);
	void updateLipoInfo();
	void showBatteryInfo();
};

#endif /* MY_LCD2_HPP */