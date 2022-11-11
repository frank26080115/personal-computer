#include "klc_config.h"
#define HISTBUFF_LEN 16

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
