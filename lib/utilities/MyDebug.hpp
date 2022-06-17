#ifndef MY_DEBUG_HPP
#define MY_DEBUG_HPP

#include <Arduino.h>
#include <ESPDateTime.h>

namespace MyLOG
{
    void LOGD(String tag, String text);
}

#endif /* MY_DEBUG_HPP */