


bool btn1_flag = false;
bool btn2_flag = false;
bool btn3_flag = false;

uint32_t sync_time = 0;
uint32_t led_time = 0;

void setup()
{
    Serial.begin(115200);

    // init buttons
    pinMode(PIN_BTN_1, INPUT_PULLUP);
    pinMode(PIN_BTN_2, INPUT_PULLUP);

    // init LEDs on power buttons
    btn_rgb32(0);
}

void btnrgb_set(uint8_t pin, uint8_t x)
{
    static int16_t prev_values = { -1, -1, -1, };
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
    
}

void loop()
{
    uint32_t now = millis();

    btn_task(now);

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
    else // is_editing == false
    {
        cfg_chunk_t* c = get_current_cfg();
        if (btn1_flag) {
            btn1_flag = false;
            c->neo_brite += 1;
            c->neo_brite %= NEOBRITE_LAST;
        }
        if (btn2_flag) {
            btn2_flag = false;
            c->btnmode += 1;
            if (pwr_mode != PWRMODE_OFF && c->btnmode == BTNMODE_OFF) {
                c->btnmode += 1;
            }
            c->btnmode %= BTNMODE_LAST;
        }
        if (btn3_flag) {
            btn3_flag = false;
            edit_cfg = get_current_cfg();
            edit_item = EDITITEM_START;
            is_editing = true;
        }
    }
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
            pwr_mode = PWRMODE_OFF_WAIT;
        }
        else if (pwr_mode == PWRMODE_OFF_WAIT)
        {
            if ((now - led_time) > 4000)
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
        else if (pwr_mode == PWRMODE_OFF_WAIT)
        {
            if ((now - led_time) > 4000)
            {
                pwr_mode = PWRMODE_ON;
            }
        }
        prev_pwrled = true;
    }
}

void show(uint32_t now, cfg_chunk_t* c, bool show_btn)
{
    uint32_t btncolour;

    uint8_t base_brite = c->neo_dir != NEODIR_PULSE_BRITE ? 0 : 64;
    uint8_t max_brite = c->neo_dir != NEODIR_PULSE_BRITE ? 255 : (64 * 3);
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
        case NEOBRITE_75:
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
        case NEOSPEED_1S: t_modu = 1000; break;
        case NEOSPEED_2S: t_modu = 2000; break;
        case NEOSPEED_3S: t_modu = 3000; break;
        case NEOSPEED_5S: t_modu = 5000; break;
        case NEOSPEED_6S: t_modu = 6000; break;
        case NEOSPEED_7S: t_modu = 7000; break;
        case NEOSPEED_8S: t_modu = 8000; break;
        case NEOSPEED_9S: t_modu = 9000; break;
        case NEOSPEED_10S: t_modu = 10000; break;
        case NEOSPEED_4S:
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
    if (top_brite > 255) { top_brite = 255; }

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

        uint32_t colour32 = strip.gamma32(strip.ColorHSV(hue, c->neo_mode == NEOMODE_WHITE ? 0 : 255, brite));
        if (i == 0 && c->btn_mode == BTNMODE_RAINBOW)
        {
            btncolour = colour32;
        }
        strip.setPixelColor(j, colour32);
    }

    strip.show();

    if (show_btn == false) {
        return;;
    }

    if (c->btn_mode == BTNMODE_RAINBOW)
    {
        btn_rgb32(btncolour);
    }
}