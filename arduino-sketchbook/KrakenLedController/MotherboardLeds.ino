#include "klc_config.h"
#define HISTBUFF_LEN 4

uint8_t pwrled_histidx = 0;
uint8_t hddled_histidx = 0;
uint16_t pwrled_histbuff[HISTBUFF_LEN];
uint16_t hddled_histbuff[HISTBUFF_LEN];
uint16_t pwrled_val, hddled_val;

void adc_init()
{
    pinMode(PIN_PWRLED, INPUT);
    pinMode(PIN_HDDLED, INPUT);
    uint16_t i;
    for (i = 0; i < HISTBUFF_LEN; i++)
    {
        pwrled_read();
        hddled_read();
    }
}

uint16_t pwrled_read()
{
    uint16_t x = analogRead(PIN_PWRLED);
    pwrled_histbuff[pwrled_histidx] = x;
    pwrled_histidx = (pwrled_histidx + 1) % HISTBUFF_LEN;
    uint32_t sum = 0;
    uint16_t i;
    for (i = 0; i < HISTBUFF_LEN; i++)
    {
        sum += pwrled_histbuff[i];
    }
    return pwrled_val = (sum / HISTBUFF_LEN);
}

uint16_t hddled_read()
{
    uint16_t x = analogRead(PIN_HDDLED);
    hddled_histbuff[hddled_histidx] = x;
    hddled_histidx = (hddled_histidx + 1) % HISTBUFF_LEN;
    uint32_t sum = 0;
    uint16_t i;
    for (i = 0; i < HISTBUFF_LEN; i++)
    {
        sum += hddled_histbuff[i];
    }
    return hddled_val = (sum / HISTBUFF_LEN);
}

void pwrcheck_task(uint32_t now)
{
    static bool prev_pwrled = false;

    if (PWRLED_IS_ON() == false)
    {
        if (prev_pwrled != false)
        {
            led_time = now;
        }

        if (pwr_mode == PWRMODE_ON)
        {
            sync_time = now;
            pwr_mode = PWRMODE_SLEEP;
        }
        else if (pwr_mode == PWRMODE_SLEEP)
        {
            if ((now - led_time) > 2000)
            {
                pwr_mode = PWRMODE_OFF;
            }
        }
        prev_pwrled = false;
    }
    else // PWRLED_IS_ON
    {
        if (prev_pwrled == false)
        {
            led_time = now;
        }

        if (pwr_mode == PWRMODE_OFF)
        {
            sync_time = now;
            pwr_mode = PWRMODE_ON;
        }
        else if (pwr_mode == PWRMODE_SLEEP)
        {
            if ((now - led_time) > 2000)
            {
                pwr_mode = PWRMODE_ON;
            }
        }
        prev_pwrled = true;
    }
}
