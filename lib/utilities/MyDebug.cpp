#ifndef MY_DEBUG_CPP
#define MY_DEBUG_CPP

#include "MyDebug.hpp"

bool MyLOG::DISABLE_LOGD = false;
bool MyLOG::DISABLE_LOGLCD = true;
bool MyLOG::SAVE_LOGD = true;

void MyLOG::LOGD(String tag, String text)
{
    if (!DISABLE_LOGD)
    {
        String logStr = "[" + DateTime.toString() + "] ";
        logStr += tag + ": ";
        logStr += text;
        //Serial.print("[" + DateTime.toString() + "] ");
        //Serial.print(tag + ": ");
        Serial.println(logStr);
        //if (SAVE_LOGD) MySdCard::appendFile(SD, "log.text", logStr.c_str());
    }
}
void MyLOG::LOGLCD(String tag, String text)
{
    if (!DISABLE_LOGLCD)
    {
        //M5.Lcd.print("[" + DateTime.toString() + "] ");
        M5.Lcd.print(tag + ": ");
        M5.Lcd.println(text);
    }
}

#endif /* MY_DEBUG_CPP */