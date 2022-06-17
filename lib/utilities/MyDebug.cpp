#ifndef MY_DEBUG_CPP
#define MY_DEBUG_CPP

#include "MyDebug.hpp"

void MyLOG::LOGD(String tag, String text)
{
    Serial.print("[" + DateTime.toString() + "] ");
    Serial.print(tag + ": ");
    Serial.println(text);
}

#endif /* MY_DEBUG_CPP */