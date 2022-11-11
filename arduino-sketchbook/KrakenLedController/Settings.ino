#include <avr/eeprom.h>

#define SETTINGS_MAGIC 0x1234

extern cfg_t cfg;
cfg_t cfg_cache;

void settings_load()
{
    eeprom_read_block((void*)&cfg, 0, sizeof(cfg_t));
    uint16_t chk_calc = fletcher16((uint8_t*)&cfg, sizeof(cfg_t) - sizeof(uint16_t));
    if ((chk_calc ^ SETTINGS_MAGIC) != cfg.chksum)
    {
        settings_default();
    }
    memcpy((void*)&cfg_cache, (void*)&cfg, sizeof(cfg_t));
}

void settings_default()
{
    memset((void*)&cfg, 0, sizeof(cfg_t));
}

void settings_save()
{
    uint16_t chk_calc = fletcher16((uint8_t*)&cfg, sizeof(cfg_t) - sizeof(uint16_t));
    cfg.chksum = chk_calc ^ SETTINGS_MAGIC;
    eeprom_update_block((void*)&cfg, 0, sizeof(cfg_t));
    memcpy((void*)&cfg_cache, (void*)&cfg, sizeof(cfg_t));
}

void settings_saveIfNeeded(uint32_t now)
{
    static uint32_t last_time = 0;
    if ((now - last_time) > 5000)
    {
        if (memcmp((void*)&cfg_cache, (void*)&cfg, sizeof(cfg_t)) != 0)
        {
            settings_save();
        }
        last_time = now;
    }
}

// https://en.wikipedia.org/wiki/Fletcher%27s_checksum
uint16_t fletcher16(uint8_t* data, int count)
{
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    int index;

    for (index = 0; index < count; ++index)
    {
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }

    return (sum2 << 8) | sum1;
}
