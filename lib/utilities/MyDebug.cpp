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

#endif /* MY_DEBUG_CPP */