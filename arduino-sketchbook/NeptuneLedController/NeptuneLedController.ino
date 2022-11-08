#include <Adafruit_NeoPixel.h>

#define LEDIN_POWER_POS 15
#define LEDIN_POWER_NEG 14
#define LEDIN_HDD_POS 13
#define LEDIN_HDD_NEG 12
#define LEDIN_POWER_5V 4

#define BUTTON_PIN 16
#define LEDOUT_RED 9
#define NEOPIXEL_PIN 17

#define NEOPIXEL_CNT 70

#define NEOPIXEL_FRAMERATE (1000 / 60)
#define NEOPIXEL_HUE_GAP (0x10000 / NEOPIXEL_CNT)
#define NEOPIXEL_HUE_ADD ((0x10000 / NEOPIXEL_CNT) / 8)
#define NEOPIXEL_BRIGHTNESS_FULL  64
#define NEOPIXEL_BRIGHTNESS_HALF  32
#define NEOPIXEL_BRIGHTNESS_LOW   8

//#define SLEEP_USE_FLASHES
#define SLEEP_USE_BICOLOR

#define PWRSTATE_SLEEP_PULSE 3000
#define PWRSTATE_ON_TIMEOUT (PWRSTATE_SLEEP_PULSE * 2)
#define PWRSTATE_OFF_TIMEOUT (PWRSTATE_SLEEP_PULSE * 2)
#define HDD_LED_TIMEOUT 500

#define BTN_TIME_DEBOUNCE 100
#define BTN_TIME_LONGPRESS 3000

enum
{
  PWRSTATE_UNKNOWN,
  PWRSTATE_ON,
  PWRSTATE_OFF,
  PWRSTATE_SLEEP,
};

enum
{
  NEOSTATE_SAFE,
  NEOSTATE_HALF_1,
  NEOSTATE_FULL_1,
  NEOSTATE_HALF_2,
  NEOSTATE_FULL_2,
  NEOSTATE_HALF_3,
  NEOSTATE_FULL_3,
  NEOSTATE_HALF_4,
  NEOSTATE_FULL_4,
  NEOSTATE_OFF,
};

char pwr_state = PWRSTATE_UNKNOWN;
char neo_state = 0;

uint32_t neopixel_time = 0;
uint32_t neopixel_rate = 100;

uint32_t pwrstate_on_time = 0;
uint32_t pwrstate_off_time = 0;
uint32_t pwrstate_sleep_time = 0;

uint32_t hdd_led_time = 0;

uint32_t btn_time = 0;
bool btn_waitforrelease = false;

Adafruit_NeoPixel pixels(NEOPIXEL_CNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(LEDIN_POWER_POS, INPUT);
  pinMode(LEDIN_POWER_NEG, INPUT);
  pinMode(LEDIN_HDD_POS, INPUT);
  pinMode(LEDIN_HDD_NEG, INPUT);
  pinMode(LEDIN_POWER_5V, INPUT);

  #ifdef INPUT_PULLUP
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  #else
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(BUTTON_PIN, HIGH);
  #endif

  pixels.begin();
  pixels.clear();
}

void loop() {
  uint32_t now = millis();
  bool draw_neopixels = false;
  bool pwr_led = isPowerLedOn();
  bool hdd_led = isHddLedOn();
  bool btn = isBtnPressed();
  static bool prev_pwr_led = false;
  static bool prev_hdd_led = false;
  static bool prev_btn = false;

  static uint32_t hue = 0;

  if (neo_state == NEOSTATE_SAFE || neo_state == NEOSTATE_OFF)
  {
    // blink heartbeat only when neopixels are not active
    if ((now % 2000) < 300) {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }

  if ((now - neopixel_time) > NEOPIXEL_FRAMERATE)
  {
    neopixel_time = now;
    draw_neopixels = true;
  }

  if (pwr_led != false && prev_pwr_led == false) // blink on event
  {
    #ifdef SLEEP_USE_FLASHES
    if (pwr_state != PWRSTATE_SLEEP) {
      Serial.printf("PWR LED ON\r\n");
    }
    else {
      Serial.printf("PWR LED ON (SLEEP)\r\n");
    }
    pwrstate_on_time = now;
    if ((pwrstate_on_time - pwrstate_off_time) < PWRSTATE_SLEEP_PULSE) {
      pwr_state = PWRSTATE_SLEEP;
      pwrstate_sleep_time = now;
      Serial.printf("SLEEP\r\n");
    }
    #else
    Serial.printf("PWR LED ON\r\n");
    #endif
  }
  else if (pwr_led == false && prev_pwr_led != false)
  {
    Serial.printf("PWR LED OFF\r\n");
    pwrstate_off_time = now;
  }

  if (hdd_led != false) {
    pwr_state = PWRSTATE_ON;
    hdd_led_time = now;
    pwrstate_on_time = pwrstate_off_time = pwrstate_sleep_time = now;
    digitalWrite(LEDOUT_RED, HIGH);
  }
  else
  {
    if ((now - hdd_led_time) > HDD_LED_TIMEOUT) {
      digitalWrite(LEDOUT_RED, LOW);
    }
  }

  if (pwr_led != false)
  {
    #ifdef SLEEP_USE_FLASHES
    if ((now - pwrstate_on_time) > PWRSTATE_ON_TIMEOUT)
    #endif
    {
      if (pwr_state != PWRSTATE_ON) {
        Serial.printf("PWR STATE ON\r\n");
      }
      pwr_state = PWRSTATE_ON;
    }
  }
  else if (pwr_led == false)
  {
    #ifdef SLEEP_USE_BICOLOR
    if (isPowerLedReversed())
    {
      if (pwr_state != PWRSTATE_SLEEP) {
        Serial.printf("SLEEP\r\n");
      }
      pwr_state = PWRSTATE_SLEEP;
      pwrstate_sleep_time = now;
    }
    else
    #endif
    if ((now - pwrstate_sleep_time) > PWRSTATE_OFF_TIMEOUT || (now - pwrstate_off_time) > PWRSTATE_OFF_TIMEOUT)
    {
      if (pwr_state != PWRSTATE_OFF) {
        Serial.printf("PWR STATE OFF\r\n");
      }
      pwr_state = PWRSTATE_OFF;
    }
  }

  if (btn != false && prev_btn == false) { // press event
    btn_time = now;
  }

  if (btn != false && (now - btn_time) > BTN_TIME_LONGPRESS)
  {
    if (btn_waitforrelease == false) {
      Serial.printf("LONG PRESS\r\n");
    }
    neo_state = NEOSTATE_OFF;
    btn_waitforrelease = true;
  }

  if (btn == false && prev_btn != false) // release event
  {
    if (btn_waitforrelease == false)
    {
      if ((now - btn_time) > BTN_TIME_DEBOUNCE)
      {
        Serial.printf("SHORT PRESS\r\n");
        neo_state += 1;
        if (neo_state >= NEOSTATE_OFF) {
          neo_state = 0;
        }
      }
    }
    else {
      btn_waitforrelease = false;
      Serial.printf("LONG PRESS RELEASED\r\n");
    }
  }

  if (draw_neopixels != false)
  {
    uint8_t brightness;
    if (neo_state == NEOSTATE_SAFE) {
      brightness = NEOPIXEL_BRIGHTNESS_LOW;
    }
    else if (neo_state == NEOSTATE_FULL_1 || neo_state == NEOSTATE_FULL_2 || neo_state == NEOSTATE_FULL_3 || neo_state == NEOSTATE_FULL_4) {
      brightness = NEOPIXEL_BRIGHTNESS_FULL;
    }
    else if (neo_state == NEOSTATE_HALF_1 || neo_state == NEOSTATE_HALF_2 || neo_state == NEOSTATE_HALF_3 || neo_state == NEOSTATE_HALF_4) {
      brightness = NEOPIXEL_BRIGHTNESS_HALF;
    }
    else if (neo_state >= NEOSTATE_OFF) {
      brightness = 0;
    }
    pixels.setBrightness(brightness);

    for (uint16_t i = 0; i < NEOPIXEL_CNT; i++)
    {
      uint16_t j = i;
      uint32_t h = i;
      uint32_t c;
      h *= NEOPIXEL_HUE_GAP;
      h += hue;
      h &= 0xFFFF;
      if (neo_state <= NEOSTATE_FULL_2 || neo_state <= NEOSTATE_HALF_2) {
        h = (0x10000 - h - 1);
      }
      c = pixels.gamma32(pixels.ColorHSV(h));
      if (pwr_state == PWRSTATE_SLEEP)
      {
        if ((i % 2) == 0) {
          c = pixels.Color(0, 0, 255);
        }
      }
      if (neo_state == NEOSTATE_FULL_2 || neo_state == NEOSTATE_HALF_2 || neo_state == NEOSTATE_FULL_4 || neo_state == NEOSTATE_HALF_4) {
        j = NEOPIXEL_CNT - i - 1;
      }
      pixels.setPixelColor(j, c);
    }
    hue += NEOPIXEL_HUE_ADD;

    pixels.show();
  }

  prev_pwr_led = pwr_led;
  prev_hdd_led = hdd_led;
  prev_btn = btn;
}

bool isPowerLedOn() {
  return digitalRead(LEDIN_POWER_POS) != LOW && digitalRead(LEDIN_POWER_NEG) == LOW;
}

bool isPowerLedReversed() {
  return digitalRead(LEDIN_POWER_POS) == LOW && digitalRead(LEDIN_POWER_NEG) != LOW;
}

bool isHddLedOn() {
  return digitalRead(LEDIN_HDD_POS) != LOW && digitalRead(LEDIN_HDD_NEG) == LOW;
}

bool isBtnPressed() {
  return digitalRead(BUTTON_PIN) == LOW;
}

