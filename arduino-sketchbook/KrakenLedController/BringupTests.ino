#include "klc_config.h"
#include "klc_defs.h"

void tests()
{
    //test_report_all_inputs();
    //test_btns();
    //test_report_power_state();
    //test_btn_rainbow();
    //test_neopixels();
}

void test_report_all_inputs()
{
    while (true)
    {
        Serial.print(millis(), DEC);
        Serial.print(F(": "));
        Serial.print(analogRead(PIN_PWRLED), DEC);
        Serial.print(F(" ; "));
        Serial.print(analogRead(PIN_HDDLED), DEC);
        Serial.print(F(" ; "));
        Serial.print(digitalRead(PIN_BTN_UPPER), DEC);
        Serial.print(F(" ; "));
        Serial.print(digitalRead(PIN_BTN_LOWER), DEC);
        Serial.println();
        delay(250);
    }
}

void test_btns()
{
    while (true)
    {
        uint32_t now = millis();
        btn_task(now);
        if (btnboth_flag) {
            Serial.print("BTN BOTH : ");
            Serial.print(millis(), DEC);
            Serial.println();
            btnboth_flag = false;
        }
        if (btnupper_flag) {
            Serial.print("BTN UP   : ");
            Serial.print(millis(), DEC);
            Serial.println();
            btnupper_flag = false;
        }
        if (btnlower_flag) {
            Serial.print("BTN DOWN : ");
            Serial.print(millis(), DEC);
            Serial.println();
            btnlower_flag = false;
        }
    }
}

extern uint8_t pwr_mode;

void test_report_power_state()
{
    while (true)
    {
        uint32_t now = millis();
        pwrled_read();
        hddled_read();
        pwrcheck_task(now);
        Serial.print(millis(), DEC);
        Serial.print(F(": "));
        Serial.print(pwr_mode, DEC);
        Serial.println();
        delay(100);
    }
}

void test_btn_rainbow()
{
    while (true)
    {
        uint32_t now = millis();
        now *= 4;
        now %= NEOPIXEL_HUE_RANGE;
        uint32_t c = strip.gamma32(strip.ColorHSV(now, 255, 255));
        btn_rgb32(c);
    }
}

void test_neopixels()
{
    uint32_t limit = 30;
    uint32_t i;
    while (true)
    {
        uint32_t now = millis();
        now *= 4;
        now %= NEOPIXEL_HUE_RANGE;
        for(i = 0; i < NEOPIXEL_CNT; i++)
        {
            if (i < limit)
            {
                uint32_t hue = NEOPIXEL_HUE_RANGE / limit;
                hue *= i;
                hue += now;
                hue %= NEOPIXEL_HUE_RANGE;
                uint32_t c = strip.gamma32(strip.ColorHSV(hue, 255, 64));
                strip.setPixelColor(i, c);
            }
            else
            {
                strip.setPixelColor(i, (uint32_t)0);
            }
        }
        strip.show();
        while (strip.canShow() == false) {
            delay(1);
        }
    }
}