#ifndef POWER_SAVING_CPP
#define POWER_SAVING_CPP

#include "PowerSaving.hpp"

using namespace MyLOG;

const String PowerSaving::TAG = "PowerSaving";
int PowerSaving::lcdState = 1;
int PowerSaving::ledState = 1;

void PowerSaving::setup()
{
    /*
    //M5.begin();  // Init M5Core.
    // M5.Lcd.setTextColor(YELLOW);  // Set the font color to yellow.
    // M5.Lcd.setTextSize(2);  // Set the font size.
    M5.Lcd.setCursor(65, 10);         // Move the cursor position to (x, y).
    //M5.Lcd.println("Button example"); // The screen prints the formatted string and wraps the line.
    M5.Lcd.setCursor(3, 35);
    //M5.Lcd.println("Press button B for 700ms");
    //M5.Lcd.println("to clear screen.");
    // M5.Lcd.setTextColor(RED);
    */
}

void PowerSaving::enable()
{
    LOGD(TAG, "enabling power save......");
    //LOGLCD(TAG, "enabling power save......");
    delay(2000);
    M5.Lcd.sleep();
    M5.Axp.SetLcdVoltage(0);
    PowerSaving::lcdState = 0;
    M5.Axp.SetLed(0);
    //MyLOG::DISABLE_LOGLCD = true;
}

void PowerSaving::disable()
{
    M5.Lcd.wakeup();
    M5.Axp.SetLcdVoltage(3000);
    PowerSaving::lcdState = 1;
    M5.Axp.SetLed(1);
    //MyLOG::DISABLE_LOGLCD = false;
}

void PowerSaving::loop()
{
    M5.update(); // Read the press state of the key.
    if (M5.BtnA.wasReleased() || M5.BtnA.pressedFor(1000, 200))
    {
        if (PowerSaving::lcdState)
        {
            enable();
        }
        else
        {
            disable();
        }
    }
    else if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(1000, 200))
    {
        // MyLOG::DISABLE_LOGLCD = !MyLOG::DISABLE_LOGLCD;
    }
    else if (M5.BtnC.wasReleased() || M5.BtnC.pressedFor(1000, 200))
    {
        M5.Lcd.clear(BLACK); // Clear the screen and set white to the background color.
        M5.Lcd.setCursor(0, 0);
    }
    else if (M5.BtnB.wasReleasefor(700))
    {
        M5.Lcd.clear(BLACK); // Clear the screen and set white to the background color.
        M5.Lcd.setCursor(0, 0);
    }
}

#endif /* POWER_SAVING_CPP */