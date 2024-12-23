#ifndef MY_LCD_CPP
#define MY_LCD_CPP

#include "MyLcd.hpp"

using namespace MyLOG;

const String MyLcd::TAG = "MyLcd";

void MyLcd::setup()
{
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(2);
}

void MyLcd::showBatteryInfo(float volt, float current, float cellDiff, float temparature1, float temparature2, int capacityRemain)
{
    LOGD(TAG, "show volt: " + String(volt));
    LOGD(TAG, "show current: " + String(current));
    LOGD(TAG, "show Lipo voltage: " + String(M5.Axp.GetBatVoltage()));
    char str[16];

    M5.Lcd.clear();
    //M5.Lcd.drawRect(1, 1, 319, 239, YELLOW);
    // M5.Lcd.setTextDatum(2);
    //M5.Axp.SetLcdVoltage(3000);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(1, 10, 7);
    M5.Lcd.setTextColor(GREEN, BLACK);
    sprintf(str, "%05.2f", volt);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("V");
    sprintf(str, "%05.2f", current);
    M5.Lcd.setCursor(158, 10, 7);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("A");
    M5.Lcd.setCursor(46, 67, 7);
    sprintf(str, "%03.0f", cellDiff);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("mV");
    M5.Lcd.setCursor(202, 67, 7);
    sprintf(str, "%03d", capacityRemain);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("%");
    M5.Lcd.setCursor(1, 124, 7);
    sprintf(str, "%05.2f", temparature1);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("C");
    M5.Lcd.setCursor(158, 124, 7);
    sprintf(str, "%05.2f", temparature2);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("C");
    M5.Lcd.setCursor(1, 181, 7);
    sprintf(str, "%05.2f", M5.Axp.GetBatVoltage());
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("V");
    M5.Lcd.setCursor(158, 181, 7);
    sprintf(str, "%05.2f", M5.Axp.GetBatCurrent());
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("A");
}

#endif /* MY_MQTT_CPP */