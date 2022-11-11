// using Arduino 1.18.15
// using Adafruit SAMD Boards v1.7.5

#include "Adafruit_TinyUSB.h" // v1.15.0

// we are pretending we are a Adafruit QT Py because the Seeeduino Xiao's TinyUSB implementation is broken
// but the pin mapping is not the same, so remap the pins on the Xiao to the numbers on the QT Py
#define XIAOPIN_0  0
#define XIAOPIN_1  2
#define XIAOPIN_2  10
#define XIAOPIN_3  8
#define XIAOPIN_4  17
#define XIAOPIN_5  9
//#define XIAOPIN_6 -1
//#define XIAOPIN_7 -1
#define XIAOPIN_8  7
#define XIAOPIN_9  3
#define XIAOPIN_10 6
#define XIAOPIN_11 15
#define XIAOPIN_12 11
#define XIAOPIN_13 5

#define PIN_LED_HEARTBEAT XIAOPIN_13
#define PIN_LED_BLUE1     XIAOPIN_12
#define PIN_LED_BLUE2     XIAOPIN_11
#define PIN_PWM_FAN       XIAOPIN_8
#define PIN_PWM_PUMP      XIAOPIN_5

#define PINSTATE_LED_ON  LOW
#define PINSTATE_LED_OFF HIGH

#define PWM_MAX_VAL  255
#define PWM_PUMP_MIN 128

#define PUMP_BOOST_TIME   2000
#define PWM_RAMP_MAX_TIME 5000
#define WATCHDOG_TIMEOUT  (30 * 1000)
#define HEARTBEAT_PERIOD  2500
#define HEARTBEAT_DUTY    300
#define RXLED_PULSE       300

enum
{
    CMD_SET_PWM_BOTH,
    CMD_SET_PWM_FAN,
    CMD_SET_PWM_PUMP,
    CMD_SAFE_SHUTDOWN,
};

enum
{
    OPMODE_SAFE,
    OPMODE_NORMAL,
    OPMODE_SHUTDOWN,
};

uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_GENERIC_INOUT(64) };
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, true);

uint8_t op_mode = OPMODE_SAFE;

uint32_t rx_led_time  = 0;
uint32_t rx_data_time = 0;

uint32_t pump_start_time = 0;

uint8_t pwm_fan_tgt   = 0;
uint8_t pwm_pump_tgt  = 0;
uint8_t pwm_fan_cur   = 0;
uint8_t pwm_pump_cur  = 0;
uint8_t pwm_fan_prev  = 0;
uint8_t pwm_pump_prev = 0;

uint32_t pump_boost_time = 0;

bool safe_shutdown = false;

void setup()
{
    // all pins setup
    pinMode     (PIN_LED_HEARTBEAT, OUTPUT);
    pinMode     (PIN_LED_BLUE1    , OUTPUT);
    pinMode     (PIN_LED_BLUE2    , OUTPUT);
    digitalWrite(PIN_LED_HEARTBEAT, PINSTATE_LED_OFF);
    digitalWrite(PIN_LED_BLUE1    , PINSTATE_LED_OFF);
    digitalWrite(PIN_LED_BLUE2    , PINSTATE_LED_OFF);
    pinMode     (PIN_PWM_FAN      , OUTPUT);
    pinMode     (PIN_PWM_PUMP     , OUTPUT);
    digitalWrite(PIN_PWM_FAN      , LOW);
    digitalWrite(PIN_PWM_PUMP     , LOW);

    // USB setup
    TinyUSBDevice.setID(0x12BA, 0x4444);
    TinyUSBDevice.setManufacturerDescriptor("frank26080115");
    TinyUSBDevice.setProductDescriptor("Kraken Cooling Controller");
    usb_hid.setReportCallback(get_report_callback, set_report_callback);
    usb_hid.begin();

    #ifdef ALLOW_ADAFRUIT_CDC
    Serial.begin(115200);
    #endif
}

void loop()
{
    static uint32_t last_tick = 0;
    uint32_t now = millis();

    // blink LEDs to indicate status
    now = (now == 0) ? 1 : now;
    uint32_t now_modu = now % HEARTBEAT_PERIOD;
    digitalWrite(PIN_LED_HEARTBEAT, (now_modu < HEARTBEAT_DUTY
                                    || (TinyUSBDevice.mounted() && now_modu >= (HEARTBEAT_DUTY * 2) && now_modu <= (HEARTBEAT_DUTY * 3))
                                    )
                                    ? PINSTATE_LED_ON : PINSTATE_LED_OFF);
    now_modu = (now_modu + (HEARTBEAT_DUTY * 4)) % HEARTBEAT_PERIOD;
    digitalWrite(PIN_LED_BLUE2,     ((op_mode == OPMODE_SAFE && now_modu < HEARTBEAT_DUTY)
                                    || (op_mode == OPMODE_SHUTDOWN && (now_modu < HEARTBEAT_DUTY || (now_modu >= (HEARTBEAT_DUTY * 2) && now_modu <= (HEARTBEAT_DUTY * 3))))
                                    )
                                    ? PINSTATE_LED_ON : PINSTATE_LED_OFF);

    // end the RX activity LED pulse
    if (rx_led_time != 0 && (now - rx_led_time) >= RXLED_PULSE) {
        rx_led_time = 0;
        digitalWrite(PIN_LED_BLUE1, PINSTATE_LED_OFF);
    }

    if (op_mode != OPMODE_SHUTDOWN)
    {
        if (rx_data_time != 0 && (now - rx_data_time) >= WATCHDOG_TIMEOUT)
        {
            // no communication from computer, timeout, go into safe mode
            rx_data_time = now;
            pwm_fan_tgt  = PWM_MAX_VAL / 4;
            pwm_pump_tgt = PWM_PUMP_MIN + 8;
            op_mode = OPMODE_SAFE;
        }
    }
    else // OPMODE_SHUTDOWN
    {
        pwm_fan_tgt  = 0;
        pwm_pump_tgt = 0;
    }

    if ((pwm_pump_prev <= 0 || pwm_pump_cur <= 0) && pwm_pump_tgt > 0)
    {
        // pump start from off state, perform a full power boost
        pump_boost_time = now;
        pwm_pump_cur = PWM_MAX_VAL;
    }
    else if (pump_boost_time != 0 && (now - pump_boost_time) < PUMP_BOOST_TIME)
    {
        // still performing pump boost
        pwm_pump_cur = PWM_MAX_VAL;
    }
    else {
        pump_boost_time = 0;
    }

    if ((now - last_tick) > (PWM_RAMP_MAX_TIME / PWM_MAX_VAL))
    {
        // it's time to increment or decrement actual PWM value for smooth ramping
        last_tick = now;
        if (pwm_fan_tgt > pwm_fan_cur && pwm_fan_cur < PWM_MAX_VAL) {
            pwm_fan_cur += 1;
        }
        else if (pwm_fan_tgt < pwm_fan_cur && pwm_fan_cur > 0) {
            pwm_fan_cur -= 1;
        }
        if (pwm_pump_tgt > pwm_pump_cur && pwm_pump_cur < PWM_MAX_VAL) {
            pwm_pump_cur += 1;
            pwm_pump_cur = (pwm_pump_cur < PWM_PUMP_MIN) ? PWM_PUMP_MIN : pwm_pump_cur; // impose minimum pump voltage
        }
        else if (pwm_pump_tgt < pwm_pump_cur && pwm_pump_cur > 0 && pump_boost_time == 0) {
            pwm_pump_cur -= 1;
            pwm_pump_cur = (pwm_pump_cur < PWM_PUMP_MIN) ? 0 : pwm_pump_cur; // impose minimum pump voltage
        }
    }

    // set the values into the PWM timers if needed
    if (pwm_fan_cur != pwm_fan_prev) {
        analogWrite(PIN_PWM_FAN, pwm_fan_cur);
        if (pwm_fan_cur <= 0) {
            digitalWrite(PIN_PWM_FAN, LOW);
        }
    }
    if (pwm_pump_cur != pwm_pump_prev) {
        analogWrite(PIN_PWM_PUMP, pwm_pump_cur);
        if (pwm_pump_cur <= 0) {
            digitalWrite(PIN_PWM_PUMP, LOW);
        }
    }

    #ifdef ALLOW_ADAFRUIT_CDC
    if (pwm_fan_cur != pwm_fan_prev || pwm_pump_cur != pwm_pump_prev)
    {
        Serial.print(now, DEC);
        Serial.print(": FAN = ");
        Serial.print(pwm_fan_cur, DEC);
        Serial.print(" ; PUMP = ");
        Serial.print(pwm_pump_cur, DEC);
        Serial.println();
    }
    #endif

    pwm_fan_prev  = pwm_fan_cur;
    pwm_pump_prev = pwm_pump_cur;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t get_report_callback (uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // not used in this example
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    // This example doesn't use multiple report and report ID
    (void) report_id;
    (void) report_type;

    // indicate RX activity using LED
    uint32_t now = millis();
    now = (now == 0) ? 1 : now;
    rx_led_time = now;
    digitalWrite(PIN_LED_BLUE1, PINSTATE_LED_ON);

    // find the data by finding magic header
    int i;
    for (i = 0; i < bufsize; i++) {
        if (buffer[i] == 0xAA && buffer[i + 1] == 0x55) {
            i += 2;
            break;
        }
    }

    if (i >= bufsize) { // no valid data found
        return;
    }

    uint8_t cmd_byte = buffer[i];
    if (cmd_byte == CMD_SAFE_SHUTDOWN)
    {
        op_mode = OPMODE_SHUTDOWN;
        pwm_fan_tgt  = 0;
        pwm_pump_tgt = 0;
        rx_data_time = now; // valid command feeds watchdog
    }
    else
    {
        op_mode = OPMODE_NORMAL;
    }

    switch (cmd_byte)
    {
        case CMD_SET_PWM_BOTH:
            pwm_fan_tgt  = buffer[i + 1];
            pwm_pump_tgt = buffer[i + 2];
            rx_data_time = now; // valid command feeds watchdog
            break;
        case CMD_SET_PWM_FAN:
            pwm_fan_tgt  = buffer[i + 1];
            rx_data_time = now; // valid command feeds watchdog
            break;
        case CMD_SET_PWM_PUMP:
            pwm_pump_tgt = buffer[i + 1];
            pwm_pump_tgt = (pwm_pump_tgt < PWM_PUMP_MIN && pwm_pump_tgt > 0) ? PWM_PUMP_MIN : pwm_pump_tgt; // impose minimum pump voltage
            rx_data_time = now; // valid command feeds watchdog
            break;
    };
}
