#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Adafruit_BMP085.h>

// Default addresses: LCD backpacks are commonly 0x27 or 0x3F.
const uint8_t LCD_ADDRESS = 0x27;
const uint8_t TOUCH_PIN = 2;
const unsigned long DISPLAY_INTERVAL_MS = 1000;
const unsigned long SERIAL_INTERVAL_MS = 10000;

LiquidCrystal_I2C lcd(LCD_ADDRESS, 20, 4);
RTC_DS3231 rtc;
Adafruit_BMP085 bmp;

bool rtcAvailable = false;
bool bmpAvailable = false;
bool lcdAvailable = false;
bool serialReporting = true;
bool touchBacklightEnabled = true;
bool backlightOn = true;
bool previousTouchState = false;
char firstLine[21] = "";
bool firstLineCustom = false;
unsigned long lastDisplay = 0;
unsigned long lastSerialReport = 0;
char inputLine[64];
uint8_t inputLength = 0;
char renderedLines[4][21] = {};
bool displayInitialized = false;

void setCustomLine(char *target, bool &enabled, const char *text, uint8_t row);

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP                         show this help"));
  Serial.println(F("  SCAN                         scan I2C bus"));
  Serial.println(F("  STATUS                       show device status"));
  Serial.println(F("  READ                         print current readings"));
  Serial.println(F("  SETTIME YYYY-MM-DD HH:MM:SS set RTC time"));
  Serial.println(F("  SYNC YYYY-MM-DD HH:MM:SS    alias for SETTIME"));
  Serial.println(F("  REPORT ON|OFF                periodic serial readings"));
  Serial.println(F("  BACKLIGHT ON|OFF             manually control LCD backlight"));
  Serial.println(F("  TOUCH ON|OFF                 enable/disable touch backlight toggle"));
  Serial.println(F("  LCDLINE FIRST <text>         set/clear first LCD row"));
}

void scanI2C() {
  Serial.println(F("I2C scan:"));
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  found 0x"));
      if (address < 16) Serial.print('0');
      Serial.println(address, HEX);
      found++;
    }
  }
  if (!found) Serial.println(F("  no devices found"));
  Serial.println(F("Scan complete."));
}

void printDateTime(const DateTime &now) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u:%02u",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  Serial.print(buffer);
}

void printReadings() {
  Serial.print(F("Time: "));
  if (rtcAvailable) printDateTime(rtc.now());
  else Serial.print(F("unavailable"));
  Serial.println();

  if (bmpAvailable) {
    Serial.print(F("Temperature: "));
    Serial.print(bmp.readTemperature(), 1);
    Serial.println(F(" C"));
    Serial.print(F("Pressure: "));
    Serial.print(bmp.readPressure() / 100.0, 1);
    Serial.println(F(" hPa"));
    Serial.print(F("Altitude: "));
    Serial.print(bmp.readAltitude(), 1);
    Serial.println(F(" m"));
  } else {
    Serial.println(F("BMP180: unavailable"));
  }
}

bool parseDateTime(const char *text, DateTime &result) {
  int year, month, day, hour, minute, second;
  if (sscanf(text, "%d-%d-%d %d:%d:%d", &year, &month, &day,
             &hour, &minute, &second) != 6) return false;
  if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
      day < 1 || day > 31 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) return false;
  result = DateTime(year, month, day, hour, minute, second);
  return result.isValid() && result.year() == (uint16_t) year &&
         result.month() == (uint8_t) month && result.day() == (uint8_t) day;
}

void setTimeFromCommand(const char *argument) {
  if (!rtcAvailable) {
    Serial.println(F("ERROR: RTC is not available."));
    return;
  }
  DateTime requested(2000, 1, 1);
  if (!parseDateTime(argument, requested)) {
    Serial.println(F("ERROR: use YYYY-MM-DD HH:MM:SS"));
    return;
  }
  rtc.adjust(requested);
  Serial.print(F("RTC set to "));
  printDateTime(rtc.now());
  Serial.println();
}

void showStatus() {
  Serial.print(F("LCD: ")); Serial.println(lcdAvailable ? F("OK") : F("not detected"));
  Serial.print(F("RTC: ")); Serial.println(rtcAvailable ? F("OK") : F("not detected"));
  Serial.print(F("BMP180: ")); Serial.println(bmpAvailable ? F("OK") : F("not detected"));
  Serial.print(F("Touch input D2: ")); Serial.println(digitalRead(TOUCH_PIN) ? F("active") : F("inactive"));
  Serial.print(F("Touch backlight: ")); Serial.println(touchBacklightEnabled ? F("enabled") : F("disabled"));
  Serial.print(F("LCD backlight: ")); Serial.println(backlightOn ? F("ON") : F("OFF"));
}

void setBacklight(bool enabled) {
  backlightOn = enabled;
  if (!lcdAvailable) return;
  if (backlightOn) lcd.backlight();
  else lcd.noBacklight();
}

void processCommand(char *line) {
  while (*line == ' ') line++;
  char command[64];
  strncpy(command, line, sizeof(command) - 1);
  command[sizeof(command) - 1] = '\0';
  for (char *p = command; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
  }
  if (strcmp(command, "HELP") == 0) printHelp();
  else if (strcmp(command, "SCAN") == 0) scanI2C();
  else if (strcmp(command, "STATUS") == 0) showStatus();
  else if (strcmp(command, "READ") == 0) printReadings();
  else if (strncmp(command, "SETTIME ", 8) == 0) setTimeFromCommand(line + 8);
  else if (strncmp(command, "SYNC ", 5) == 0) setTimeFromCommand(line + 5);
  else if (strcmp(command, "REPORT ON") == 0) { serialReporting = true; Serial.println(F("Periodic report ON")); }
  else if (strcmp(command, "REPORT OFF") == 0) { serialReporting = false; Serial.println(F("Periodic report OFF")); }
  else if (strcmp(command, "BACKLIGHT ON") == 0) { setBacklight(true); Serial.println(F("LCD backlight ON")); }
  else if (strcmp(command, "BACKLIGHT OFF") == 0) { setBacklight(false); Serial.println(F("LCD backlight OFF")); }
  else if (strcmp(command, "TOUCH ON") == 0) { touchBacklightEnabled = true; Serial.println(F("Touch backlight toggle ON")); }
  else if (strcmp(command, "TOUCH OFF") == 0) { touchBacklightEnabled = false; Serial.println(F("Touch backlight toggle OFF")); }
  else if (strncmp(command, "LCDLINE FIRST", 13) == 0) setCustomLine(firstLine, firstLineCustom, line + 13, 0);
  else if (*line) Serial.println(F("ERROR: unknown command; type HELP"));
}

void readSerial() {
  while (Serial.available()) {
    char character = Serial.read();
    if (character == '\n' || character == '\r') {
      if (inputLength) {
        inputLine[inputLength] = '\0';
        processCommand(inputLine);
        inputLength = 0;
      }
    } else if (inputLength < sizeof(inputLine) - 1) {
      inputLine[inputLength++] = character;
    }
  }
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void updateDisplay();

void setCustomLine(char *target, bool &enabled, const char *text, uint8_t row) {
  while (*text == ' ') text++;
  size_t length = strlen(text);
  while (length > 0 && text[length - 1] == ' ') length--;
  if (length == 0) {
    target[0] = '\0';
    enabled = false;
    Serial.print(F("LCD row ")); Serial.print(row); Serial.println(F(" reset"));
  } else {
    if (length > 20) length = 20;
    memcpy(target, text, length);
    target[length] = '\0';
    enabled = true;
    Serial.print(F("LCD row ")); Serial.print(row); Serial.println(F(" updated"));
  }
  updateDisplay();
}

void readTouch() {
  bool touchState = digitalRead(TOUCH_PIN) == HIGH;
  if (touchBacklightEnabled && touchState && !previousTouchState) {
    setBacklight(!backlightOn);
    Serial.print(F("Touch: LCD backlight "));
    Serial.println(backlightOn ? F("ON") : F("OFF"));
  }
  previousTouchState = touchState;
}

void updateDisplay() {
  if (!lcdAvailable) return;
  char lines[4][21];
  char value[12];
  for (uint8_t row = 0; row < 4; row++) {
    memset(lines[row], ' ', 20);
    lines[row][20] = '\0';
  }
  if (firstLineCustom) strncpy(lines[0], firstLine, 20);
  else strncpy(lines[0], "Arduino Weather", 20);

  if (rtcAvailable) {
    DateTime now = rtc.now();
    snprintf(lines[1], sizeof(lines[1]), "%02u:%02u:%02u %04u-%02u-%02u",
             now.hour(), now.minute(), now.second(), now.year(), now.month(), now.day());
  } else {
    strncpy(lines[1], "Time: RTC unavailable", 20);
  }

  if (bmpAvailable) {
    dtostrf(bmp.readTemperature(), 5, 1, value);
    snprintf(lines[2], sizeof(lines[2]), "Temp:%s C", value);
    dtostrf(bmp.readPressure() / 100.0, 7, 1, value);
    snprintf(lines[3], sizeof(lines[3]), "Press:%s hPa", value);
  } else {
    strncpy(lines[2], "Temp: BMP unavailable", 20);
    strncpy(lines[3], "Press: BMP unavailable", 20);
  }

  for (uint8_t row = 0; row < 4; row++) {
    lines[row][20] = '\0';
    if (!displayInitialized || strcmp(lines[row], renderedLines[row]) != 0) {
      lcd.setCursor(0, row);
      lcd.print(lines[row]);
      strcpy(renderedLines[row], lines[row]);
    }
  }
  displayInitialized = true;
}

void setup() {
  Serial.begin(9600);
  pinMode(TOUCH_PIN, INPUT);
  previousTouchState = digitalRead(TOUCH_PIN) == HIGH;
  Wire.begin();
  lcdAvailable = i2cDevicePresent(LCD_ADDRESS);
  if (lcdAvailable) {
    lcd.init();
    setBacklight(true);
  }
  rtcAvailable = rtc.begin();
  bmpAvailable = bmp.begin();

  Serial.println(F("Arduino Weather Clock ready (9600 baud)"));
  showStatus();
  Serial.println(F("Type HELP for commands."));
  updateDisplay();
}

void loop() {
  readSerial();
  readTouch();
  unsigned long now = millis();
  if (now - lastDisplay >= DISPLAY_INTERVAL_MS) {
    lastDisplay = now;
    updateDisplay();
  }
  if (serialReporting && now - lastSerialReport >= SERIAL_INTERVAL_MS) {
    lastSerialReport = now;
    printReadings();
  }
}