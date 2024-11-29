#ifndef MY_DEBUG_CPP
#define MY_DEBUG_CPP

#include "MyDebug.hpp"

bool MyLOG::DISABLE_LOGD = false;

void MyLOG::LOGD(String tag, String text)
{
    if (!DISABLE_LOGD)
    {
        Serial.print("[" + DateTime.toString() + "] ");
        Serial.print(tag + ": ");
        Serial.println(text);
    }
}
void MyLOG::LOGLCD(String tag, String text)
{
    if (!DISABLE_LOGD)
    {
        M5.Lcd.wakeup();
        M5.Axp.SetLcdVoltage(3000);
        M5.Lcd.print("[" + DateTime.toString() + "] ");
        M5.Lcd.print(tag + ": ");
        M5.Lcd.println(text);
    }
}

#endif /* MY_DEBUG_CPP */