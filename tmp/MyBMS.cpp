#include <Arduino.h>
#include "MyBMS.h"

void MyBMS::LOGD(String tag, String text)
{
    Serial.print("[" + DateTime.toString() + "] ");
    Serial.print(tag + ": ");
    Serial.println(text);
}