#ifndef MY_DEBUG_HPP
#define MY_DEBUG_HPP

#include <Arduino.h>
#include <ESPDateTime.h>
#include <M5Core2.h>

namespace MyLOG
{
    extern bool DISABLE_LOGD;
    extern bool DISABLE_LOGLCD;
    void LOGD(String tag, String text);
    void LOGLCD(String tag, String text);
}

#endif /* MY_DEBUG_HPP */