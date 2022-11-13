#include "klc_config.h"
#include "klc_defs.h"

#include <avr/wdt.h>

#include <Adafruit_NeoPixel.h> // using https://github.com/adafruit/Adafruit_NeoPixel/commit/7fe11e4c404834b1e727a026498532458d38701f

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_CNT, PIN_NEOPIX, NEO_GRB + NEO_KHZ800);

// indicates button has been pressed
bool btnupper_flag = false;
bool btnlower_flag = false;
bool btnboth_flag  = false;

uint32_t sync_time   = 0; // used to align start of fading with power-off event
uint32_t pwrled_time = 0; // the last time that the pwr LED has changed state

uint8_t  pwr_mode;                      // current computer power mode
uint8_t  edit_item     = EDITITEM_LAST; // the item being edited
bool     is_editing    = false;         // if we are in editing mode
uint32_t edit_act_time = 0;             // used to timeout editing mode

cfg_t        cfg;      // configuration for the whole device
cfg_chunk_t* edit_cfg; // the configuration being edited

uint8_t test_mode = 0; // used to override the power mode through serial port

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

    // tests();

    // this device might run for years on end, enable watchdog
    wdt_enable(WDTO_4S);
}

void loop()
{
    if (strip.canShow() == false) {
        // I know this is an unneccessary delay but I'm too lazy to do real button debouncing
        return;
    }

    wdt_reset(); // feed the watchdog

    uint32_t now = millis();

    handle_serial();

    // sample all inputs
    btn_task(now);
    pwrled_read();
    hddled_read();
    pwrcheck_task(now);

    // force a test mode if required
    if (test_mode != 0) {
        pwr_mode = test_mode - 1;
    }

    if (is_editing)
    {
        if (btnupper_flag)
        {
            btnupper_flag = false; // indicated this has been handled
            edit_act_time = now;   // prevent timeout
            // upper button changes the setting that is currently being edited
            switch(edit_item)
            {
                case EDITITEM_NEO_MODE:
                    edit_cfg->neo_mode += 1;
                    edit_cfg->neo_mode %= NEOMODE_LAST;
                    Serial.print(F("Neo_Mode: "));
                    Serial.print(edit_cfg->neo_mode, DEC);
                    Serial.println();
                    break;
                case EDITITEM_NEO_SPEED:
                    edit_cfg->neo_speed += 1;
                    edit_cfg->neo_speed %= NEOSPEED_LAST;
                    Serial.print(F("Neo_Speed: "));
                    Serial.print(edit_cfg->neo_speed, DEC);
                    Serial.println();
                    break;
                case EDITITEM_NEO_DIR:
                    edit_cfg->neo_dir += 1;
                    edit_cfg->neo_dir %= NEODIR_LAST;
                    Serial.print(F("Neo_Dir: "));
                    Serial.print(edit_cfg->neo_dir, DEC);
                    Serial.println();
                    break;
                case EDITITEM_NEO_SPAN:
                    edit_cfg->neo_span += 1;
                    edit_cfg->neo_span %= NEOSPAN_LAST;
                    Serial.print(F("Neo_Span: "));
                    Serial.print(edit_cfg->neo_span, DEC);
                    Serial.println();
                    break;
            }
        }
        if (btnlower_flag)
        {
            btnlower_flag = false; // indicated this has been handled
            edit_act_time = now;   // prevent timeout
            // lower button changes which setting is being edited
            edit_item += 1;
            Serial.print(F("Edit Mode Next Item: "));
            Serial.print(edit_item, DEC);
            Serial.println();
            if (edit_item >= EDITITEM_LAST) {
                is_editing = false;
                Serial.println(F("Exiting Edit Mode"));
            }
        }
        if (btnboth_flag)
        {
            btnboth_flag = false; // indicated this has been handled
            edit_act_time = now;  // prevent timeout
            edit_item = 0;
            Serial.println(F("Edit Mode Looping Again, Item 0"));
        }
        if ((now - edit_act_time) >= EDIT_MODE_TIMEOUT)
        {
            is_editing = false;
            Serial.println(F("Timeout Edit Mode"));
        }
    }

    if (is_editing == false)
    {
        cfg_chunk_t* c = get_current_cfg();

        // when we are in normal operating conditions, pressing the upper button changes the brightness of the neopixel strip
        if (btnupper_flag) {
            btnupper_flag = false;
            c->neo_brite += 1;
            c->neo_brite %= NEOBRITE_LAST;
            Serial.print(F("NeoPixels Brightness: "));
            Serial.print(c->neo_brite, DEC);
            Serial.println();
        }

        // when we are in normal operating conditions, pressing the upper button changes the button mode
        if (btnlower_flag) {
            btnlower_flag = false;
            c->btn_mode += 1;
            c->btn_mode %= BTNMODE_LAST;
            Serial.print(F("Btn Mode: "));
            Serial.print(c->btn_mode, DEC);
            Serial.println();
        }

        // if both buttons are pressed, enter editing mode
        if (btnboth_flag) {
            btnboth_flag = false;
            edit_cfg = get_current_cfg();
            edit_item = EDITITEM_START;
            is_editing = true;
            edit_act_time = now; // prevent timeout
            Serial.println(F("Entering Edit Mode, Item 0"));
        }
        show(now, c, true);

        settings_saveIfNeeded(now);
    }
    else // in editing mode
    {
        // show the LED strip but not the button
        show(now, edit_cfg, false);
        // indicate the currently edited item by blinking the button a certain colour
        uint32_t btn_hue = NEOPIXEL_HUE_RANGE / (EDITITEM_LAST + 1);
        btn_hue *= edit_item;
        uint32_t btn_colour = strip.ColorHSV(btn_hue, 255, 255);
        btn_rgb32(((now % 600) < 300) ? btn_colour : 0); // blink
    }
}

void handle_serial()
{
    while (Serial.available() > 0) {
        char c = Serial.read();
        switch (c)
        {
            // use serial port keystrokes to fake button presses
            // because the buttons are recessed, hard to press
            case 'q': btnupper_flag = true; Serial.println(F("BTN Ser U"));     break;
            case 'a': btnlower_flag = true; Serial.println(F("BTN Ser D"));     break;
            case 'z': btnboth_flag  = true; Serial.println(F("BTN Ser B"));     break;
            // use serial port keystrokes to pretend to be in a certain power mode
            // for easier programming while PC is off
            case '1': test_mode = 1;        Serial.println(F("Test Mode 1"));   break;
            case '2': test_mode = 2;        Serial.println(F("Test Mode 2"));   break;
            case '3': test_mode = 3;        Serial.println(F("Test Mode 3"));   break;
            case '0': test_mode = 0;        Serial.println(F("Test Mode Off")); break;
            // report the current settings
            case 'r':
                cfg_chunk_t* rep_cfg = is_editing ? edit_cfg : get_current_cfg();
                Serial.println(F("Settings:"));
                Serial.print(F("  mode: "));
                Serial.print(rep_cfg->neo_mode, DEC);
                Serial.print(F("  speed: "));
                Serial.print(rep_cfg->neo_speed, DEC);
                Serial.print(F("  dir: "));
                Serial.print(rep_cfg->neo_dir, DEC);
                Serial.print(F("  span: "));
                Serial.print(rep_cfg->neo_span, DEC);
                Serial.print(F("  brite: "));
                Serial.print(rep_cfg->neo_brite, DEC);
                Serial.print(F("  btn: "));
                Serial.print(rep_cfg->btn_mode, DEC);
                Serial.println();
        }
    }
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
    return NULL;
}

void show(uint32_t now, cfg_chunk_t* c, bool show_btn)
{
    uint32_t btn_hue;

    uint8_t base_brite = c->neo_dir != NEODIR_PULSE_BRITE ? 0 : (NEOPIXEL_MAX_BRITE / 4); // used for the fading with base brightness
    uint8_t max_brite  = NEOPIXEL_MAX_BRITE - base_brite;
    uint32_t top_brite = max_brite;

    uint8_t strip_brite = c->neo_mode == NEOMODE_WHITE ? (NEOPIXEL_MAX_BRITE / 4) : NEOPIXEL_MAX_BRITE;
    // white mode needs to be dimmed, the power draw is too much

    // set the overall brightness of the strip
    switch (c->neo_brite)
    {
        case NEOBRITE_100:
            break;
        case NEOBRITE_75:
            strip_brite /= 2;
            strip_brite += strip_brite / 2;
            break;
        case NEOBRITE_50:
            strip_brite /= 2;
            break;
        case NEOBRITE_25:
            strip_brite /= 4;
            break;
        case NEOBRITE_0:
        default:
            strip_brite = 0;
            break;
    }
    strip.setBrightness(strip_brite);

    // the speed setting is setting the period time of one cycle
    uint32_t t_modu;
    switch (c->neo_speed)
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
        // fixed colour
        hue_start = NEOPIXEL_HUE_RANGE;
        hue_start *= (15 * (c->neo_mode - NEOMODE_HUE_0));
        hue_start /= 360;
    }
    else if (c->neo_mode == NEOMODE_RAINBOW)
    {
        // starting colour depending on what part of the cycle we are in
        hue_start = NEOPIXEL_HUE_RANGE;
        hue_start *= t;
        hue_start /= t_modu;
    }

    if (c->btn_mode == BTNMODE_RAINBOW)
    {
        // starting colour depending on what part of the cycle we are in
        btn_hue = NEOPIXEL_HUE_RANGE;
        btn_hue *= t;
        btn_hue /= t_modu;
    }

    if (c->neo_dir == NEODIR_PULSE || c->neo_dir == NEODIR_PULSE_BRITE)
    {
        // calc the phase from time
        double xi = t * M_PI * 2;
        xi /= t_modu;
        xi += M_PI;

        // cos waveform goes from 1 to -1 to 1, we want it to go from 0 to 1 to 0, so first shift it up and then invert it
        double b = cos(xi) + 1;
        double br = top_brite;
        br *= 1 - (b / 2);
        top_brite = (uint32_t)lround(br);
    }
    top_brite = (top_brite > NEOPIXEL_MAX_BRITE) ? NEOPIXEL_MAX_BRITE : top_brite; // impose limit

    int i;
    for (i = 0; i < NEOPIXEL_CNT; i++) // for all pixels
    {
        int j = i;
        if (c->neo_dir == NEODIR_RIGHT)
        {
            // flip everything if direction is flipped
            j = NEOPIXEL_CNT - 1 - i;
        }

        uint32_t hue = hue_start;
        if (c->neo_mode == NEOMODE_RAINBOW && c->neo_span != NEOSPAN_0 && (c->neo_dir == NEODIR_LEFT || c->neo_dir == NEODIR_RIGHT))
        {
            uint32_t hue_offset = NEOPIXEL_HUE_RANGE;
            hue_offset /= NEOPIXEL_CNT;

            // change the offset based on how many full rainbows need to fit within the strip
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
            hue_offset *= i;
            hue = hue_start + hue_offset;
        }
        hue %= NEOPIXEL_HUE_RANGE; // fit in range

        uint32_t brite = top_brite;
        if ((c->neo_dir == NEODIR_LEFT || c->neo_dir == NEODIR_RIGHT) && c->neo_mode != NEOMODE_RAINBOW)
        {
            // calculate phase of a particular pixel
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
            double xoff = t * M_PI * 2;
            xoff /= t_modu;
            xi += xoff;

            // cos waveform goes from 1 to -1 to 1, we want it to go from 0 to 1 to 0, so first shift it up and then invert it
            double m = cos(xi) + 1;
            m /= 2;
            double br = top_brite;
            br *= (1.0 - m);
            brite = (uint32_t)lround(br);
        }
        brite += base_brite; // add the minimum
        brite = brite > NEOPIXEL_MAX_BRITE ? NEOPIXEL_MAX_BRITE : brite; // impose limit

        if (j == 5 && c->btn_mode == BTNMODE_RAINBOW && c->neo_mode == BTNMODE_RAINBOW)
        {
            // if button needs to be rainbow too, sync its colour with the pixel near the button
            btn_hue = hue;
        }

        uint32_t colour32 = strip.gamma32(strip.ColorHSV(hue, c->neo_mode == NEOMODE_WHITE ? 0 : 255, brite));
        strip.setPixelColor(j, colour32);
    }

    strip.show();

    if (show_btn == false) {
        return;
    }

    //t_modu = 4000;
    //t = now % t_modu;

    top_brite = 255;
    strip.setBrightness(NEOPIXEL_MAX_BRITE);
    if (pwr_mode != PWRMODE_ON)
    {
        double xi = t * M_PI * 2;
        xi /= t_modu;
        double b = cos(xi) + 1;
        double br = top_brite;
        br *= b / 2;
        top_brite = (uint32_t)lround(br);
    }
    top_brite = (top_brite > 255) ? 255 : top_brite;

    if (c->btn_mode == BTNMODE_RAINBOW)
    {
        // the hue was calculated earlier
        btn_rgb32(strip.gamma32(strip.ColorHSV(btn_hue, 255, NEOPIXEL_MAX_BRITE)));
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
        btn_hue = NEOPIXEL_HUE_RANGE;
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
            analogWrite(pin, 255 - x); // LED is active low, common 
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
