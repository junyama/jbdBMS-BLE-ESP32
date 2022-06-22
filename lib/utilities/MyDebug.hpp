#ifndef MY_DEBUG_HPP
#define MY_DEBUG_HPP

#include <Arduino.h>
#include <ESPDateTime.h>

namespace MyLOG
{
    extern bool DISABLE_LOGD;
    void LOGD(String tag, String text);
}

#endif /* MY_DEBUG_HPP */