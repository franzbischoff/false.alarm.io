#include <Arduino.h>
#include <EEPROM.h>

#define a0 A7
#define a1 A6
#define a2 A0
#define a3 A1
#define a4 A2
#define a5 A3
#define a6 A4
#define a7 A5
#define evt0 2
#define evt1 0
#define act 3

// analog pins
int apin[] = {a0, a1, a2, a3, a4, a5, a6, a7};

unsigned short cnt0;
unsigned short cnt1;
unsigned short cnt2;
unsigned short cnt3;
unsigned short cnt4;
unsigned short cnt5;
unsigned short cnt6;
unsigned short cnt7;

byte buf[3];

long us_remain = 0;
unsigned long us_now;
unsigned long us_next = micros();
unsigned long us_act_last = 0;
bool act_last_val = false;
unsigned long us_wait = 10000;

#define MAX_SERIAL_BUF 100
char serialBuf[MAX_SERIAL_BUF]; // a string to hold incoming data
unsigned short serialPos = 0;

unsigned short cal_hi[8] = {922, 922, 922, 922, 922, 922, 922, 922}; // 4v
unsigned short cal_lo[8] = {102, 102, 102, 102, 102, 102, 102, 102}; // -4v

void setup() {
  // 0v, 4v read calibration value
  for (int ch = 0; ch < 8; ch++) {
    unsigned short cnt = 0;
    EEPROM.get(4 * ch, cal_lo[ch]);
    EEPROM.get(4 * ch + 2, cal_hi[ch]);
  }

  analogReference(INTERNAL); // 2.56V reference voltage
  pinMode(evt0, INPUT_PULLUP);
  pinMode(evt1, INPUT_PULLUP);
  pinMode(act, OUTPUT);
  pinMode(a0, INPUT);
  pinMode(a1, INPUT);
  pinMode(a2, INPUT);
  pinMode(a3, INPUT);
  pinMode(a4, INPUT);
  pinMode(a5, INPUT);
  pinMode(a6, INPUT);
  pinMode(a7, INPUT);
  Serial.begin(57600);
}

// serialBuf: Stored statement
// serialPos: length of sentence
void processCmd() {
  if (serialPos < 3)
    return;                    // invalid command
  if (serialBuf[0] == 's') {   // set
    if (serialBuf[1] == 'r') { // sampling rate
      serialBuf[6] = '\0';     // safety check
      unsigned long srate = atoi(serialBuf + 2);
      if (srate > 0 && srate <= 1000)
        us_wait = 1000000ul / srate;
    }
  } else if (serialBuf[0] == 'c') {                   // cl, ch, cp
    if (serialBuf[1] == 'l' || serialBuf[1] == 'h') { // 0v or 4v calibration
      Serial.println();
      for (int ch = 0; ch < 8; ch++) {
        Serial.print("CH");
        Serial.print(ch);
        Serial.print("=");

        // average
        long sum = 0;
        for (int t = 0; t < 1000; t++)
          sum += analogRead(apin[ch]);
        short cnt = sum / 1000;

        // save
        if (serialBuf[1] == 'l') {
          EEPROM.put(4 * ch, cnt);
          cal_lo[ch] = cnt;
        } else if (serialBuf[1] == 'h') {
          EEPROM.put(4 * ch + 2, cnt);
          cal_hi[ch] = cnt;
        }

        // print
        Serial.println(cnt);
      }
    } else if (serialBuf[1] == 'p') {
      for (int ch = 0; ch < 8; ch++) {
        Serial.print(cal_lo[ch]);
        Serial.print('\t');
        Serial.println(cal_hi[ch]);
      }
    }
  }
}

// SerialEvent occurs whenever a new data comes in the hardware serial RX. This
// routine is run between each time loop() runs, so using delay inside loop can
// delay response. Multiple bytes of data may be available.
void serialEvent() {
  while (Serial.available()) {
    // get the new byte
    char inChar = (char)Serial.read();
    if (inChar == '\r' || inChar == '\n') {
      // if the incoming character is a newline, set a flag so the main loop can
      // do something about it:
      serialBuf[serialPos++] = '\0';
      processCmd();
      serialPos = 0;
    } else {
      // or add it to the serialBuf:
      serialBuf[serialPos++] = inChar;
    }
    if (serialPos == MAX_SERIAL_BUF)
      serialPos = 0;
  }
}

unsigned short readAndCalibrate(int ch) {
  float cnt = (float)analogRead(apin[ch]);
  cnt = 102.4 + (819.2 * (cnt - cal_lo[ch])) / (cal_hi[ch] - cal_lo[ch]);
  if (cnt < 0)
    return 0;
  else if (cnt > 1023)
    return 1023;
  return (unsigned short)cnt;
}

void loop() {

#if defined(ARDUINO_AVR_LEONARDO) || defined(ARDUINO_AVR_YUN) || defined(ARDUINO_AVR_MICRO) ||                         \
    defined(ARDUINO_ARCH_SAMD)
  // Since serialEvent is not called automatically here, call it manually.
  serialEvent();
#endif

  us_now = micros(); // 500 Hz 2ms = 2000 us
  us_remain = long(us_next - us_now);

  if (us_remain > 0)
    return; // not yet reached

  // I waited long enough. Next call? Otherwise, if the execution time is longer than us_wait, the call will continue.
  us_next = max(us_next + us_wait, us_now);

  if ((us_now & 0xFFF00000) != us_act_last) {
    digitalWrite(act, act_last_val ? LOW : HIGH);
    act_last_val = !act_last_val;
    us_act_last = us_now & 0xFFF00000;
  }

  cnt0 = readAndCalibrate(0);
  cnt1 = readAndCalibrate(1);
  buf[0] = cnt0 & 0x7F;
  buf[1] = cnt1 & 0x7F;
  buf[2] = (cnt0 & 0x380) >> 3;
  buf[2] |= (cnt1 & 0x380) >> 6;
  buf[0] |= 0x80;
  if (digitalRead(evt0))
    buf[2] &= 0xFE;
  else
    buf[2] |= 1;
  Serial.write(buf, 3);

  cnt0 = readAndCalibrate(2);
  cnt1 = readAndCalibrate(3);
  buf[0] = cnt0 & 0x7F;
  buf[1] = cnt1 & 0x7F;
  buf[2] = (cnt0 & 0x380) >> 3;
  buf[2] |= (cnt1 & 0x380) >> 6;
  if (digitalRead(evt1))
    buf[2] &= 0xFE;
  else
    buf[2] |= 1;
  Serial.write(buf, 3);

  cnt0 = readAndCalibrate(4);
  cnt1 = readAndCalibrate(5);
  buf[0] = cnt0 & 0x7F;
  buf[1] = cnt1 & 0x7F;
  buf[2] = (cnt0 & 0x380) >> 3;
  buf[2] |= (cnt1 & 0x380) >> 6;
  Serial.write(buf, 3);

  cnt0 = readAndCalibrate(6);
  cnt1 = readAndCalibrate(7);
  buf[0] = cnt0 & 0x7F;
  buf[1] = cnt1 & 0x7F;
  buf[2] = (cnt0 & 0x380) >> 3;
  buf[2] |= (cnt1 & 0x380) >> 6;
  Serial.write(buf, 3);
}
