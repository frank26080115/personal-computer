#define PIN_FAN_PWM  12
#define PIN_PUMP_PWM 4
#define PIN_FAN_ADC  A4
#define PIN_PUMP_ADC A5

uint32_t ser_time = 0;
char ser_prev;
uint32_t dbg_time = 0;

#ifdef CORE_TEENSY_RAWHID
byte hid_buff[64];
uint8_t hid_read, hid_itr;
#endif

void setup() {

  #ifndef CORE_TEENSY_RAWHID
  Serial.begin(9600);
  #endif

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(PIN_FAN_PWM, OUTPUT);
  digitalWrite(PIN_FAN_PWM, LOW);
  pinMode(PIN_PUMP_PWM, OUTPUT);
  digitalWrite(PIN_PUMP_PWM, LOW);

  TCCR4B = (TCCR4B & 0xF0) | 0x03;
  OCR4C = 182;
}

void loop() {
  uint32_t now = millis();
  uint8_t temp_ocr4c = OCR4C;
  
  bool blink = false;
  uint32_t blinktime = now % 3000;

  if (blinktime < 150) {
    blink = true;
  }
  else if (ser_time != 0 && blinktime > 300 && blinktime < 450) {
    blink = true;
  }
  
  if (blink != false) {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  #ifdef CORE_TEENSY_RAWHID
  hid_read = RawHID.recv(hid_buff, 0);
  for (hid_itr = 0; hid_itr < hid_read; hid_itr++)
  {
    char c = hid_buff[hid_itr];
  #else
  while (Serial.available() > 0)
  {
    char c = Serial.read();
  #endif

    if ((ser_prev == 'P' || ser_prev == 'F' || ser_prev == 'p' || ser_prev == 'f') && (c >= '0' && c <= '9'))
    {
      ser_time = now;
      uint32_t v, dest;
      
      if (ser_prev == 'P' || ser_prev == 'p') {
        dest = PIN_PUMP_PWM;
      }
      else if (ser_prev == 'F' || ser_prev == 'f') {
        dest = PIN_FAN_PWM;
      }
      
      if (c == '0')
      {
        v = 0;
      }
      else if (c == '9')
      {
        v = (dest == PIN_FAN_PWM) ? temp_ocr4c : 255;
      }
      else
      {
        v = map(c, '0', '9', 0, (dest == PIN_FAN_PWM) ? temp_ocr4c : 255);
      }
      v = constrain(v, 0, 255);
      
      #ifndef CORE_TEENSY_RAWHID
      Serial.printf("SER CMD %c = %d\r\n", ser_prev, v);
      #endif
      if (dest == PIN_FAN_PWM && v >= temp_ocr4c)
      {
        digitalWrite(PIN_FAN_PWM, HIGH);
      }
      else
      {
        if (dest == PIN_FAN_PWM && v < 3) {
          v = 3;
        }
        analogWrite(dest, (uint8_t)v);
      }
    }
    ser_prev = c;
  }

  if ((now - ser_time) > (30000) || ser_time == 0)
  {
    uint32_t nowSec = now / 1000;
    uint8_t fanVal, pumpVal;
    ser_time = 0;

    fanVal = controlOutput(PIN_FAN_PWM, PIN_FAN_ADC, 0, temp_ocr4c);
    if (fanVal == temp_ocr4c) {
      digitalWrite(PIN_FAN_PWM, HIGH);
    }
    else
    {
      if (fanVal < 3) {
        fanVal = 3;
      }
      analogWrite(PIN_FAN_PWM, fanVal);
    }
    /*
    else if (fanVal <= 0) {
      digitalWrite(PIN_FAN_PWM, LOW);
    }
    else {
      analogWrite(PIN_FAN_PWM, fanVal);
    }
    */
    pumpVal = controlOutput(PIN_PUMP_PWM, PIN_PUMP_ADC, 60, 255);
    analogWrite(PIN_PUMP_PWM, pumpVal);

    if (nowSec != dbg_time) {
      dbg_time = nowSec;
      #ifndef CORE_TEENSY_RAWHID
      Serial.printf("FAN %d ; PUMP %d\r\n", fanVal, pumpVal);
      #endif
    }
  }
}

uint8_t controlOutput(uint8_t outPin, uint8_t inPin, uint8_t minVal, uint8_t maxVal)
{
  signed long ana = analogRead(inPin);
  signed long v = map(ana, 1024 / 16, 1024 - (1024 / 16), minVal, maxVal);
  v = constrain(v, 0, maxVal);
  if (v <= minVal) {
    return 0;
  }
  return v;
}
