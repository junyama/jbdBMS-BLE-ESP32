#ifndef POWER_SAVING_HPP
#define POWER_SAVING_HPP

#include "M5Core2.h"
#include "MyDebug.hpp"
#include <Arduino.h>

#define MSG_BUFFER_SIZE (50)

class PowerSaving
{
private:
	static const String TAG;

public:
	static int lcdState;
	static int ledState;

	static void setup();
	static void loop();
	static void enable();
	static void disable();
};

#endif /* POWER_SAVING_HPP */