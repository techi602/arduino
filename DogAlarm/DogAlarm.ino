/**
 * Tracks elapsed time when you last time walked your dog
 */

#include "SevenSegmentTM1637.h"
#include "SevenSegmentExtended.h"

const byte PIN_CLK = 4;   // define CLK pin (any digital pin)
const byte PIN_DIO = 5;   // define DIO pin (any digital pin)

const byte PIN_LED_RED = 11;
const byte PIN_LED_YELLOW = 12;
const byte PIN_LED_GREEN = 13;

SevenSegmentExtended      display(PIN_CLK, PIN_DIO);

void showLed(byte pin)
{
  digitalWrite(PIN_LED_RED, pin == PIN_LED_RED ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, pin == PIN_LED_YELLOW ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, pin == PIN_LED_GREEN ? HIGH : LOW);
}

void setup() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  Serial.begin(9600);         // initializes the Serial connection @ 9600 baud
  display.begin();            // initializes the display
  display.setBacklight(100);  // set the brightness to 100 %

  int flashDelay = 500;
  for (byte i = 0; i < 3; i++) {
    showLed(PIN_LED_RED);
    delay(flashDelay);
    showLed(PIN_LED_YELLOW);
    delay(flashDelay);
    showLed(PIN_LED_GREEN);
    delay(flashDelay);
  }
}

void showLedByTime(int hours)
{
  if (hours < 8) {
    showLed(PIN_LED_GREEN);
  } else if (hours < 12) {
    showLed(PIN_LED_YELLOW);
  } else {
    showLed(PIN_LED_RED);
  }
}

void loop() {

  unsigned long t_milli = millis();

  int days, hours, mins, secs;
  int fractime;
  unsigned long inttime;

  inttime  = t_milli / 1000;
  fractime = t_milli % 1000;
  // inttime is the total number of number of seconds
  // fractimeis the number of thousandths of a second

  // number of days is total number of seconds divided by 24 divided by 3600
  days     = inttime / (24 * 3600);
  inttime  = inttime % (24 * 3600);

  // Now, inttime is the remainder after subtracting the number of seconds
  // in the number of days
  hours    = inttime / 3600;
  inttime  = inttime % 3600;

  // Now, inttime is the remainder after subtracting the number of seconds
  // in the number of days and hours
  mins     = inttime / 60;
  inttime  = inttime % 60;

  // Now inttime is the number of seconds left after subtracting the number
  // in the number of days, hours and minutes. In other words, it is the
  // number of seconds.
  secs = inttime;

  display.printTime(hours, mins, true);
  showLedByTime(hours);
  delay(60000);
};
