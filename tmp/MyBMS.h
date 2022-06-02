#ifndef MY_BMS
#define MY_BMS

#include <Arduino.h>
#include "ESPDateTime.h"
#include "mydatatypes.h"

class MyBMS
{
public:
    static void LOGD(String tag, String text);
};

#endif /* MY_BMS */