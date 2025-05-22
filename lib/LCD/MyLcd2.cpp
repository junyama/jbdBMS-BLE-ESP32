#ifndef MY_LCD2_CPP
#define MY_LCD2_CPP

#include "MyLcd2.hpp"

using namespace MyLOG;

void MyLcd2::setup()
{
    M5.Lcd.setTextFont(1);
    M5.Lcd.setTextSize(2);
    height = M5.Lcd.height();
    width = M5.Lcd.width();
}

void MyLcd2::println(String text)
{
    if (M5.Lcd.getCursorY() > height - 10)
    {
        M5.Lcd.clear();
        M5.Lcd.setCursor(1, 1);
    }
    if (isBatteryInfoShown)
    {
        M5.Lcd.clear();
        M5.Lcd.setTextSize(1);
        isBatteryInfoShown = false;
    }
    M5.Lcd.println(text);
}

void MyLcd2::updateBmsInfo(int bmsIndex, float volt, float current, float cellDiff, float temparature1, float temparature2, int capacityRemain)
{
    LOGD(TAG, "showBatteryInfo called with bmsIndex: " + String(bmsIndex));
    LOGD(TAG, "show volt: " + String(volt));
    LOGD(TAG, "show current: " + String(current));

    bmsInfoArr[bmsIndex].volt = volt;
    bmsInfoArr[bmsIndex].current = current;
    bmsInfoArr[bmsIndex].cellDiff = cellDiff;
    bmsInfoArr[bmsIndex].temparature1 = temparature1;
    bmsInfoArr[bmsIndex].temparature2 = temparature2;
    bmsInfoArr[bmsIndex].capacityRemain = capacityRemain;

    showBatteryInfo();
}

void MyLcd2::showBatteryInfo()
{
    LOGD(TAG, "showBatteryInfo() called with bmsIndex: " + String(bmsIndexShown));
    char str[16];
    M5.Lcd.clear();
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 1, 2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.print(bmsInfoArr[bmsIndexShown].deviceName);
    M5.Lcd.setCursor(1, 17, 7);
    M5.Lcd.setTextColor(GREEN, BLACK);
    sprintf(str, "%05.2f", bmsInfoArr[bmsIndexShown].volt);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("V");
    sprintf(str, "%05.2f", bmsInfoArr[bmsIndexShown].current);
    M5.Lcd.setCursor(158, 17, 7);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("A");
    M5.Lcd.setCursor(46, 73, 7);
    sprintf(str, "%03.0f", bmsInfoArr[bmsIndexShown].cellDiff);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("mV");
    M5.Lcd.setCursor(202, 73, 7);
    sprintf(str, "%03d", bmsInfoArr[bmsIndexShown].capacityRemain);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("%");
    M5.Lcd.setCursor(1, 131, 7);
    sprintf(str, "%05.2f", bmsInfoArr[bmsIndexShown].temparature1);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("C");
    M5.Lcd.setCursor(158, 131, 7);
    sprintf(str, "%05.2f", vMaterLipoInfo.vMater);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("V");
    M5.Lcd.setCursor(1, 192, 7);
    sprintf(str, "%05.2f", vMaterLipoInfo.lipoVolt);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("V");
    M5.Lcd.setCursor(158, 192, 7);
    sprintf(str, "%05.2f", vMaterLipoInfo.lipoCurrent);
    M5.Lcd.print(str);
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("A");
    isBatteryInfoShown = true;
}

void MyLcd2::updateVoltMaterInfo(float volt)
{
    vMaterLipoInfo.vMater = volt;
}

void MyLcd2::updateLipoInfo()
{
    vMaterLipoInfo.lipoVolt = M5.Axp.GetBatVoltage();
    vMaterLipoInfo.lipoCurrent = M5.Axp.GetBatCurrent();
}

/*
void MyLcd2::loop()
{
    M5.update(); // Read the press state of the key.
    if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(1000, 200))
    {
        if (++bmsIndexShown > 2)
            bmsIndexShown = 0;
        LOGD(TAG, "BtnB pushed with bmsIndex: " + String(bmsIndexShown));
        showBatteryInfo();
    }
}
*/

#endif /* MY_LCD2_CPP */