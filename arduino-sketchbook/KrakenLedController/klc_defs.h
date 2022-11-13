#ifndef _KLC_DEFS_H_
#define _KLC_DEFS_H_

#include "klc_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265359
#endif

#define NEOPIXEL_MAX_BRITE 255
#define NEOPIXEL_HUE_RANGE 65536

enum
{
    EDITITEM_START,
    EDITITEM_NEO_MODE = EDITITEM_START,
    EDITITEM_NEO_SPEED,
    EDITITEM_NEO_DIR,
    EDITITEM_NEO_SPAN,
    EDITITEM_LAST,
};

enum
{
    NEOMODE_RAINBOW,
    NEOMODE_HUE_0,
    NEOMODE_HUE_15,
    NEOMODE_HUE_30,
    NEOMODE_HUE_45,
    NEOMODE_HUE_60,
    NEOMODE_HUE_75,
    NEOMODE_HUE_90,
    NEOMODE_HUE_105,
    NEOMODE_HUE_120,
    NEOMODE_HUE_135,
    NEOMODE_HUE_150,
    NEOMODE_HUE_165,
    NEOMODE_HUE_180,
    NEOMODE_HUE_195,
    NEOMODE_HUE_210,
    NEOMODE_HUE_225,
    NEOMODE_HUE_240,
    NEOMODE_HUE_255,
    NEOMODE_HUE_270,
    NEOMODE_HUE_285,
    NEOMODE_HUE_300,
    NEOMODE_HUE_315,
    NEOMODE_HUE_330,
    NEOMODE_HUE_345,
    NEOMODE_WHITE,
    NEOMODE_LAST,
};

enum
{
    NEODIR_LEFT,
    NEODIR_CONST,
    NEODIR_PULSE,
    NEODIR_PULSE_BRITE,
    NEODIR_RIGHT,
    NEODIR_LAST,
};

enum
{
    NEOBRITE_25,
    NEOBRITE_50,
    NEOBRITE_75,
    NEOBRITE_100,
    NEOBRITE_0,
    NEOBRITE_LAST,
};

enum
{
    NEOSPEED_4S,
    NEOSPEED_5S,
    NEOSPEED_6S,
    NEOSPEED_7S,
    NEOSPEED_8S,
    NEOSPEED_9S,
    NEOSPEED_10S,
    NEOSPEED_1S,
    NEOSPEED_2S,
    NEOSPEED_3S,
    NEOSPEED_LAST,
};

enum
{
    NEOSPAN_100,
    NEOSPAN_150,
    NEOSPAN_200,
    NEOSPAN_250,
    NEOSPAN_300,
    NEOSPAN_25,
    NEOSPAN_50,
    NEOSPAN_0,
    NEOSPAN_LAST,
};

enum
{
    BTNMODE_RAINBOW,
    BTNMODE_HUE_0,
    BTNMODE_HUE_15,
    BTNMODE_HUE_30,
    BTNMODE_HUE_45,
    BTNMODE_HUE_60,
    BTNMODE_HUE_75,
    BTNMODE_HUE_90,
    BTNMODE_HUE_105,
    BTNMODE_HUE_120,
    BTNMODE_HUE_135,
    BTNMODE_HUE_150,
    BTNMODE_HUE_165,
    BTNMODE_HUE_180,
    BTNMODE_HUE_195,
    BTNMODE_HUE_210,
    BTNMODE_HUE_225,
    BTNMODE_HUE_240,
    BTNMODE_HUE_255,
    BTNMODE_HUE_270,
    BTNMODE_HUE_285,
    BTNMODE_HUE_300,
    BTNMODE_HUE_315,
    BTNMODE_HUE_330,
    BTNMODE_HUE_345,
    BTNMODE_WHITE,
    BTNMODE_OFF,
    BTNMODE_LAST,
};

typedef struct
{
    uint8_t neo_mode;
    uint8_t neo_speed;
    uint8_t neo_dir;
    uint8_t neo_span;
    uint8_t neo_brite;
    uint8_t btn_mode;
}
cfg_chunk_t;

typedef struct
{
    cfg_chunk_t norm;
    cfg_chunk_t sleep;
    cfg_chunk_t off;
    uint16_t chksum;
}
cfg_t;

enum
{
    PWRMODE_ON,
    PWRMODE_SLEEP,
    PWRMODE_OFF,
};

extern uint16_t pwrled_val, hddled_val;

#define PWRLED_IS_ON() (pwrled_val < (256 * 3))
#define HDDLED_IS_ON() (hddled_val < (256 * 3))

#define BTNUPPER_IS_DOWN() (digitalRead(PIN_BTN_UPPER) == LOW)
#define BTNLOWER_IS_DOWN() (digitalRead(PIN_BTN_LOWER) == LOW)

#endif
