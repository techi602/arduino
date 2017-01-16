/***************************************************************************
  LCD display with time, temperature, humidity and pressure

  Time = RTC module DS3231 with NTP sync
  Temp = BME280

  data stored in thingspeak cloud
 ***************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>
#include <RtcDS3231.h>
#include "ThingSpeak.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define SMELLSON_I2C_ADDR 0x8
#define BME_I2C_ADDR 0x76
#define countof(a) (sizeof(a) / sizeof(a[0]))
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display
//RtcDS3231 Rtc;
RtcDS3231<TwoWire> Rtc(Wire);

char ssid[] = "";
char pass[] = "";
int status = WL_IDLE_STATUS;
unsigned long myChannelNumber = 0;
const char * myWriteAPIKey = "";
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "tak.cesnet.cz", 1 * 3600, 60000);


WiFiClient  client;

long lastupdate = 0;

void setup() {


  Serial.begin(9600);
  Serial.println(F("Techi superclock"));


  Rtc.Begin();


  /*
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    Rtc.SetDateTime(compiled);
  */

  lcd.init();                      // initialize the lcd
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();
  lcd.clear();


  if (!bme.begin(BME_I2C_ADDR)) {
    lcd.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  if (!Rtc.GetIsRunning())
  {
    lcd.print("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);

  WiFi.begin(ssid, pass);
  ThingSpeak.begin(client);

  timeClient.begin();

  Wire.begin();
}


void printDateTime(const RtcDateTime& dt)
{
  char datestring[20];
  char dateOfWeek[2];

  switch (dt.DayOfWeek()) {
    case 0:
      strcpy(dateOfWeek, "NE");
      break;
    case 1:
      strcpy(dateOfWeek, "PO");
      break;
    case 2:
      strcpy(dateOfWeek, "UT");
      break;
    case 3:
      strcpy(dateOfWeek, "ST");
      break;
    case 4:
      strcpy(dateOfWeek, "CT");
      break;
    case 5:
      strcpy(dateOfWeek, "PA");
      break;
    case 6:
      strcpy(dateOfWeek, "SO");
      break;
  }
  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u.%02u. %s %02u:%02u:%02u"),

             dt.Day(),
             dt.Month(),
             dateOfWeek,
             //dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second()
            );
  lcd.print(datestring);
}



void loop() {
  long ms = millis();
  float tmp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pre = bme.readPressure() / 100.0F;

  String smellValue = "";
/*
  Wire.requestFrom(8, 6);    // request 6 bytes from slave device #8

  while (Wire.available()) { // slave may send less than requested
    char c = Wire.read(); // receive a byte as character
    smellValue += c;
  }

  int air = 100 - ((smellValue.toInt() / 4));
  if (!smellValue.length()) {
    air = 0;
  }
  */
  lcd.setCursor(0, 0);
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);

  lcd.setCursor(0, 1);
  lcd.print("TMP: ");
  lcd.print(tmp);
  lcd.print((char) 223);
  lcd.print('C');

  lcd.print(' ');
  lcd.print("WiFi");
  lcd.print(WiFi.status() == WL_CONNECTED ? " ON" : "OFF");

  lcd.setCursor(0, 2);
  lcd.print("HUM: ");
  lcd.print(hum);
  lcd.print("% ");
/*
  lcd.print("AIR: ");
  lcd.print(air);
  lcd.print('%');
  lcd.print(' ');
  */

  lcd.setCursor(0, 3);
  lcd.print("PRE: ");
  lcd.print(pre);
  lcd.print(" hPa  ");

  /*
      lcd.print("Approx. Altitude = ");
      lcd.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
      lcd.println(" m");
  */

  //lcd.setCursor(0, 4);

  if (WiFi.status() == WL_CONNECTED) {
    if (ms - lastupdate > (60 * 1000)) {
      ThingSpeak.setField(1, tmp);
      ThingSpeak.setField(2, hum);
      ThingSpeak.setField(3, pre);
      ThingSpeak.setField(4, smellValue.toInt());
      ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

      if (timeClient.update()) {
        RtcDateTime ntptime(now.Year(), now.Month(), now.Day(), timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
        if (timeClient.getSeconds() != now.Second() || timeClient.getHours() != now.Hour() || timeClient.getMinutes() != now.Minute()) {
          Rtc.SetDateTime(ntptime);
          lcd.setCursor(0, 0);
          lcd.print("NTP SYNC COMPLETE  ");
        }
      }
      lastupdate = ms;
    }

  }


  delay(900);
}
