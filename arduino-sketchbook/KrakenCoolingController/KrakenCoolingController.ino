// using Arduino 1.18.15
// using Adafruit SAMD Boards v1.7.5

#include "Adafruit_TinyUSB.h" // v1.15.0

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

uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
};

Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, true);

// the setup function runs once when you press reset or power the board
void setup()
{
    pinMode(XIAOPIN_11, OUTPUT);
    pinMode(XIAOPIN_12, OUTPUT);
    pinMode(XIAOPIN_13, OUTPUT);
    digitalWrite(XIAOPIN_11, HIGH);
    digitalWrite(XIAOPIN_12, HIGH);
    digitalWrite(XIAOPIN_13, HIGH);

    TinyUSBDevice.setID(0x1212, 0x3434);
    TinyUSBDevice.setManufacturerDescriptor("Frank");
    TinyUSBDevice.setProductDescriptor("Kraken Cooling Controller");
    usb_hid.setReportCallback(get_report_callback, set_report_callback);
    usb_hid.begin();
}

void loop()
{
    digitalWrite(XIAOPIN_11, LOW);
    delay(200);
    digitalWrite(XIAOPIN_11, HIGH);
    delay(200);
    digitalWrite(XIAOPIN_12, LOW);
    delay(200);
    digitalWrite(XIAOPIN_12, HIGH);
    delay(200);
    digitalWrite(XIAOPIN_13, LOW);
    delay(200);
    digitalWrite(XIAOPIN_13, HIGH);
    delay(500);
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

    // echo back anything we received from host
    usb_hid.sendReport(0, buffer, bufsize);
}