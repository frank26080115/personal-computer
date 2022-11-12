#include "klc_config.h"
#include "klc_defs.h"

#include <Adafruit_NeoPixel.h> // using https://github.com/adafruit/Adafruit_NeoPixel/commit/7fe11e4c404834b1e727a026498532458d38701f

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_CNT, PIN_NEOPIX, NEO_GRB + NEO_KHZ800);

bool btn1_flag = false;
bool btn2_flag = false;
bool btn3_flag = false;

uint32_t sync_time = 0;
uint32_t led_time = 0;

uint8_t pwr_mode;
uint8_t edit_item = EDITITEM_LAST;
bool is_editing = false;

cfg_t cfg;
cfg_chunk_t* edit_cfg;

void setup()
{
    Serial.begin(115200);

    // init buttons
    pinMode(PIN_BTN_UPPER, INPUT_PULLUP);
    pinMode(PIN_BTN_LOWER, INPUT_PULLUP);

    // init LEDs on power buttons
    btn_rgb32(0);

    // init NeoPixels
    strip.begin();

    settings_load();

    tests();
}

void loop()
{
    if (strip.canShow() == false) {
        return;
    }

    uint32_t now = millis();

    btn_task(now);
    pwrled_read();
    hddled_read();
    pwrcheck_task(now);

    if (is_editing)
    {
        if (btn1_flag) {
            btn1_flag = false;
        }
        if (btn1_flag) {
            btn1_flag = false;
            edit_item += 1;
            if (edit_item >= EDITITEM_LAST) {
                is_editing = false;
            }
        }
        if (btn3_flag) {
            btn3_flag = false;
            // do nothing
        }
    }

    if (is_editing == false)
    {
        cfg_chunk_t* c = get_current_cfg();
        if (btn1_flag) {
            btn1_flag = false;
            c->neo_brite += 1;
            c->neo_brite %= NEOBRITE_LAST;
        }
        if (btn2_flag) {
            btn2_flag = false;
            c->btn_mode += 1;
            if (pwr_mode != PWRMODE_OFF && c->btn_mode == BTNMODE_OFF) {
                c->btn_mode += 1;
            }
            c->btn_mode %= BTNMODE_LAST;
        }
        if (btn3_flag) {
            btn3_flag = false;
            edit_cfg = get_current_cfg();
            edit_item = EDITITEM_START;
            is_editing = true;
        }
        show(now, c, true);
    }
    else
    {
        show(now, edit_cfg, false);
        uint16_t btn_hue = 65536 / (EDITITEM_LAST + 1);
        btn_hue *= edit_item;
        uint32_t btn_colour = strip.ColorHSV(btn_colour, 255, 255);
        btn_rgb32(((now % 600) < 300) ? btn_colour : 0);
    }

    settings_saveIfNeeded(now);
}

cfg_chunk_t* get_current_cfg()
{
    if (pwr_mode == PWRMODE_ON) {
        return &(cfg.norm);
    }
    if (pwr_mode == PWRMODE_SLEEP) {
        return &(cfg.sleep);
    }
    if (pwr_mode == PWRMODE_OFF) {
        return &(cfg.off);
    }
}

void show(uint32_t now, cfg_chunk_t* c, bool show_btn)
{
    uint32_t btn_hue;

    uint8_t base_brite = c->neo_dir != NEODIR_PULSE_BRITE ? 0 : 64;
    uint8_t max_brite  = c->neo_dir != NEODIR_PULSE_BRITE ? 255 : (64 * 3);
    uint32_t top_brite;

    switch (c->neo_brite)
    {
        case NEOBRITE_100:
            top_brite = max_brite;
            break;
        case NEOBRITE_75:
            top_brite = max_brite / 4;
            top_brite *= 3;
            break;
        case NEOBRITE_50:
            top_brite = max_brite / 2;
            break;
        case NEOBRITE_25:
            top_brite = max_brite / 4;
        case NEOBRITE_0:
        default:
            top_brite = 0;
            break;
    }

    uint32_t t_modu;
    switch (c->neo_span)
    {
        case NEOSPEED_1S : t_modu = 1000; break;
        case NEOSPEED_2S : t_modu = 2000; break;
        case NEOSPEED_3S : t_modu = 3000; break;
        case NEOSPEED_5S : t_modu = 5000; break;
        case NEOSPEED_6S : t_modu = 6000; break;
        case NEOSPEED_7S : t_modu = 7000; break;
        case NEOSPEED_8S : t_modu = 8000; break;
        case NEOSPEED_9S : t_modu = 9000; break;
        case NEOSPEED_10S: t_modu = 10000; break;
        case NEOSPEED_4S :
        default:
            t_modu = 4000;
    }

    uint32_t t = now % t_modu;

    uint32_t hue_start = 0;
    if (c->neo_mode >= NEOMODE_HUE_0 && c->neo_mode <= NEOMODE_HUE_345)
    {
        hue_start = 65536;
        hue_start *= (15 * (c->neo_mode - NEOMODE_HUE_0));
        hue_start /= 360;
    }
    else if (c->neo_mode == NEOMODE_RAINBOW)
    {
        hue_start = 65536;
        hue_start *= t;
        hue_start /= t_modu;
    }

    if (c->neo_dir == NEODIR_PULSE || c->neo_dir == NEODIR_PULSE_BRITE)
    {
        double xi = t * M_PI * 2;
        xi /= t_modu;
        xi += M_PI;
        double b = cos(xi) + 1;
        double br = top_brite;
        br *= b;
        top_brite = (uint32_t)lround(br);
    }
    top_brite = (top_brite > 255) ? 255 : top_brite;

    int i;
    for (i = 0; i < NEOPIXEL_CNT; i++)
    {
        int j = i;
        if (c->neo_dir == NEODIR_RIGHT)
        {
            j = NEOPIXEL_CNT - 1 - i;
        }

        uint32_t hue = hue_start;
        if (c->neo_mode == NEOMODE_RAINBOW && c->neo_span != NEOSPAN_0)
        {
            uint32_t hue_offset = 65536;
            hue_offset *= i;
            hue_offset /= NEOPIXEL_CNT;
            if (c->neo_span == NEOSPAN_150) {
                hue_offset += hue_offset / 2;
            }
            else if (c->neo_span == NEOSPAN_200) {
                hue_offset += hue_offset;
            }
            else if (c->neo_span == NEOSPAN_250) {
                hue_offset *= 3;
                hue_offset /= 4;
            }
            else if (c->neo_span == NEOSPAN_300) {
                hue_offset *= 3;
            }
            else if (c->neo_span == NEOSPAN_50) {
                hue_offset /= 2;
            }
            else if (c->neo_span == NEOSPAN_25) {
                hue_offset /= 4;
            }
            hue = hue_start + hue_offset;
        }
        hue %= 65536;

        uint32_t brite = top_brite;
        if ((c->neo_dir == NEODIR_LEFT || c->neo_dir == NEODIR_RIGHT) && c->neo_mode != NEOMODE_RAINBOW)
        {
            double xi = i * M_PI * 2;
            switch (c->neo_span)
            {
                case NEOSPAN_25:
                    xi /= 4;
                    break;
                case NEOSPAN_50:
                    xi /= 2;
                    break;
                case NEOSPAN_150:
                    xi *= 1.5;
                    break;
                case NEOSPAN_200:
                    xi *= 2;
                    break;
                case NEOSPAN_250:
                    xi *= 2.5;
                    break;
                case NEOSPAN_300:
                    xi *= 2.5;
                    break;
                case NEOSPAN_100:
                default:
                    break;
            }
            xi /= NEOPIXEL_CNT;
            xi += M_PI;
            double m = cos(xi) + 1;
            double br = brite;
            br *= m;
            brite = (uint32_t)lround(br);
        }
        brite += base_brite;
        if (brite > 255) { brite = 255; };

        if (j == 5 && c->btn_mode == BTNMODE_RAINBOW)
        {
            btn_hue = hue;
        }

        uint32_t colour32 = strip.gamma32(strip.ColorHSV(hue, c->neo_mode == NEOMODE_WHITE ? 0 : 255, brite));
        strip.setPixelColor(j, colour32);
    }

    strip.show();

    if (show_btn == false) {
        return;;
    }

    //t_modu = 4000;
    //t = now % t_modu;

    top_brite = 255;
    if (pwr_mode != PWRMODE_ON)
    {
        double xi = t * M_PI * 2;
        xi /= t_modu;
        xi += M_PI;
        double b = cos(xi) + 1;
        double br = top_brite;
        br *= b;
        top_brite = (uint32_t)lround(br);
    }
    top_brite = (top_brite > 255) ? 255 : top_brite;

    if (c->btn_mode == BTNMODE_RAINBOW)
    {
        btn_rgb32(strip.gamma32(strip.ColorHSV(btn_hue, 255, 255)));
    }
    else if (c->btn_mode == BTNMODE_WHITE)
    {
        btn_rgb32(strip.ColorHSV(0, 0, top_brite));
    }
    else if (c->btn_mode == BTNMODE_OFF)
    {
        if (pwr_mode != PWRMODE_ON) {
            btn_rgb32(0);
        }
        else {
            btn_rgb32(strip.ColorHSV(0, 0, top_brite));
        }
    }
    else if (c->btn_mode >= BTNMODE_HUE_0 && c->btn_mode <= BTNMODE_HUE_345)
    {
        btn_hue = 65536;
        btn_hue *= (15 * (c->btn_mode - BTNMODE_HUE_0));
        btn_hue /= 360;
        btn_rgb32(strip.gamma32(strip.ColorHSV(btn_hue, 255, top_brite)));
    }
}


void btnrgb_set(uint8_t pin, uint8_t x)
{
    static int16_t prev_values[] = { -1, -1, -1, };
    uint8_t prev_val_idx;

    switch (pin)
    {
        case PIN_BTNLED_R: prev_val_idx = 0; break;
        case PIN_BTNLED_G: prev_val_idx = 1; break;
        case PIN_BTNLED_B: prev_val_idx = 2; break;
    }

    int16_t prev_v = prev_values[prev_val_idx];

    if (x == 0)
    {
        if (prev_v != 0)
        {
            digitalWrite(pin, HIGH);
            pinMode(pin, INPUT_PULLUP);
        }
    }
    else
    {
        if (prev_v <= 0)
        {
            digitalWrite(pin, HIGH);
            pinMode(pin, OUTPUT);
        }
        if (x != prev_v)
        {
            analogWrite(pin, 255 - x);
        }
    }
    prev_values[prev_val_idx] = x;
}

void btn_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    btnrgb_set(PIN_BTNLED_R, r);
    btnrgb_set(PIN_BTNLED_G, g);
    btnrgb_set(PIN_BTNLED_B, b);
}

void btn_rgb32(uint32_t colour)
{
    uint8_t r = (uint8_t)(colour >> 16);
    uint8_t g = (uint8_t)(colour >>  8);
    uint8_t b = (uint8_t) colour;
    btn_rgb(r, g, b);
}
