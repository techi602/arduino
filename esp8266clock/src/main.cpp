#include <Arduino.h>
#include "config.h"
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Adafruit_BME280.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Wemos D1 mini pin names are accepted by the ESP8266 Arduino core.
constexpr uint8_t PIN_SCL = D1;
constexpr uint8_t PIN_SDA = D2;
constexpr uint8_t PIN_LCD_RST = D3;
constexpr uint8_t PIN_LCD_CE = D4;
constexpr uint8_t PIN_LCD_DC = D5;
constexpr uint8_t PIN_LCD_DIN = D6;
constexpr uint8_t PIN_LCD_CLK = D7;
constexpr uint8_t PIN_LIGHT = D0;
constexpr uint8_t LIGHT_ON_LEVEL = LOW;
constexpr uint8_t LIGHT_OFF_LEVEL = HIGH;

constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
constexpr uint32_t DISPLAY_INTERVAL_MS = 7000;
constexpr uint32_t BOOT_DIAGNOSTICS_MS = 5000;
constexpr uint32_t BOOT_SENSOR_DATA_MS = 5000;
constexpr uint32_t PRESSURE_TREND_INTERVAL_MS = 900000;
constexpr uint32_t NTP_SYNC_INTERVAL_MS = 21600000;
constexpr uint32_t NTP_RETRY_INTERVAL_MS = 30000;
constexpr uint8_t I2C_ADDRESS_BME280 = 0x76;
constexpr uint16_t WEB_SERVER_PORT = 80;

Adafruit_BME280 bme;
RTC_DS3231 rtc;
Adafruit_PCD8544 display(PIN_LCD_CLK, PIN_LCD_DIN, PIN_LCD_DC, PIN_LCD_CE, PIN_LCD_RST);
ESP8266WebServer webServer(WEB_SERVER_PORT);

bool rtcOk = false;
bool bmeOk = false;
bool wifiRequested = false;
bool ntpConfigured = false;
bool lightOn = false;
uint8_t displayPage = 0;
uint32_t lastSensorRead = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t lastWifiAttempt = 0;
uint32_t lastNtpSync = 0;
uint32_t lastNtpAttempt = 0;
uint32_t lastPressureTrend = 0;
uint32_t bootDisplayStart = 0;
bool startupDisplayActive = true;
String serialLine;

const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Weather Clock</title><style>
body{font-family:system-ui,sans-serif;max-width:720px;margin:2rem auto;padding:0 1rem;background:#eef2f3;color:#172026}
main{background:white;border-radius:14px;padding:1.2rem;box-shadow:0 2px 12px #0002}h1{margin-top:0}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:.7rem}.card{padding:.8rem;background:#f4f7f8;border-radius:10px}
.value{font-size:1.35rem;font-weight:700}.buttons{display:flex;gap:.7rem;margin:1rem 0}button{border:0;border-radius:8px;padding:.7rem 1.2rem;font-weight:700;cursor:pointer}#on{background:#ffd166}#off{background:#d9e2e6}
small{color:#52636b}#message{min-height:1.3em}
</style></head><body><main><h1>Wemos Weather Clock</h1><p id="message">Loading...</p>
<div class="grid"><div class="card">Time<div class="value" id="time">--</div></div>
<div class="card">Temperature<div class="value"><span id="temperature">--</span> °C</div></div>
<div class="card">Humidity<div class="value"><span id="humidity">--</span> %</div></div>
<div class="card">Pressure<div class="value"><span id="pressure">--</span> hPa</div></div>
<div class="card">Dew point<div class="value"><span id="dew">--</span> °C</div></div>
<div class="card">Feels like<div class="value"><span id="feels">--</span> °C</div></div>
<div class="card">Comfort<div class="value" id="comfort">--</div></div></div>
<div class="buttons"><button id="on" onclick="setLight('on')">Light ON</button><button id="off" onclick="setLight('off')">Light OFF</button></div>
<p>Light output: <strong id="lightState">--</strong> (D0)</p><p><small>Pressure trend: <span id="trend">--</span> · Uptime: <span id="uptime">--</span></small></p>
</main><script>
const $=id=>document.getElementById(id);function show(v){$('time').textContent=v.time;$('temperature').textContent=Number(v.temperature_c).toFixed(1);$('humidity').textContent=Number(v.humidity_pct).toFixed(1);$('pressure').textContent=Number(v.pressure_hpa).toFixed(1);$('dew').textContent=Number(v.dew_point_c).toFixed(1);$('feels').textContent=Number(v.heat_index_c).toFixed(1);$('comfort').textContent=v.comfort;$('lightState').textContent=v.light_on?'ON':'OFF';$('trend').textContent=v.pressure_trend;$('uptime').textContent=v.uptime;}
async function refresh(){try{const r=await fetch('/api/data');if(!r.ok)throw Error();show(await r.json());$('message').textContent='Live data · refreshed '+new Date().toLocaleTimeString()}catch(e){$('message').textContent='Device data unavailable'}}
async function setLight(state){try{const r=await fetch('/api/light/'+state,{method:'POST'});const v=await r.json();show(v);$('message').textContent='Light switched '+state.toUpperCase()}catch(e){$('message').textContent='Could not switch light'}}refresh();setInterval(refresh,5000);
</script></body></html>)HTML";

float temperatureC = NAN;
float humidity = NAN;
float pressureHpa = NAN;
float pressureReferenceHpa = NAN;
float minTemperature = INFINITY;
float maxTemperature = -INFINITY;

float dewPointC() {
  if (!isfinite(temperatureC) || !isfinite(humidity) || humidity <= 0.0F) return NAN;
  const float gamma = log(humidity / 100.0F) + (17.62F * temperatureC) / (243.12F + temperatureC);
  return (243.12F * gamma) / (17.62F - gamma);
}

float heatIndexC() {
  if (!isfinite(temperatureC) || !isfinite(humidity)) return NAN;
  const float tempF = temperatureC * 9.0F / 5.0F + 32.0F;
  if (tempF < 80.0F || humidity < 40.0F) return temperatureC;
  const float indexF = -42.379F + 2.04901523F * tempF + 10.14333127F * humidity
                       - 0.22475541F * tempF * humidity - 0.00683783F * tempF * tempF
                       - 0.05481717F * humidity * humidity + 0.00122874F * tempF * tempF * humidity
                       + 0.00085282F * tempF * humidity * humidity
                       - 0.00000199F * tempF * tempF * humidity * humidity;
  return (indexF - 32.0F) * 5.0F / 9.0F;
}

const __FlashStringHelper *comfortLabel() {
  if (!isfinite(temperatureC) || !isfinite(humidity)) return F("unknown");
  if (temperatureC < 16.0F) return F("cool");
  if (temperatureC > 28.0F || humidity > 75.0F) return F("warm/humid");
  if (humidity < 30.0F) return F("dry");
  return F("comfortable");
}

const __FlashStringHelper *pressureTrendLabel() {
  if (!isfinite(pressureReferenceHpa) || !isfinite(pressureHpa)) return F("learning");
  const float change = pressureHpa - pressureReferenceHpa;
  if (change > 1.0F) return F("rising");
  if (change < -1.0F) return F("falling");
  return F("steady");
}

void printTwoDigits(Print &out, uint8_t value) {
  if (value < 10) out.print('0');
  out.print(value);
}

String dateTimeString(const DateTime &now) {
  char text[32];
  snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(text);
}

String uptimeString() {
  uint32_t seconds = millis() / 1000UL;
  uint32_t days = seconds / 86400UL;
  seconds %= 86400UL;
  uint8_t hours = seconds / 3600UL;
  seconds %= 3600UL;
  uint8_t minutes = seconds / 60UL;
  seconds %= 60UL;
  char text[32];
  snprintf(text, sizeof(text), "%lud %02uh %02um %02us", (unsigned long) days,
           hours, minutes, (unsigned) seconds);
  return String(text);
}

String webMeasurementsJson() {
  DateTime now = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  String json = F("{\"time\":\"");
  json += dateTimeString(now);
  json += F("\",\"temperature_c\":"); json += String(temperatureC, 2);
  json += F(" ,\"humidity_pct\":"); json += String(humidity, 2);
  json += F(" ,\"pressure_hpa\":"); json += String(pressureHpa, 2);
  json += F(" ,\"dew_point_c\":"); json += String(dewPointC(), 2);
  json += F(" ,\"heat_index_c\":"); json += String(heatIndexC(), 2);
  json += F(" ,\"comfort\":\""); json += comfortLabel(); json += F("\"");
  json += F(" ,\"pressure_trend\":\""); json += pressureTrendLabel(); json += F("\"");
  json += F(" ,\"light_on\":"); json += lightOn ? F("true") : F("false");
  json += F(" ,\"ip\":\"");
  if (WiFi.status() == WL_CONNECTED) json += WiFi.localIP().toString();
  else json += F("disconnected");
  json += F("\"");
  json += F(" ,\"uptime\":\""); json += uptimeString(); json += F("\"}");
  json.replace(" ,", ",");
  return json;
}

void handleWebLight(bool requestedState) {
  lightOn = requestedState;
  digitalWrite(PIN_LIGHT, lightOn ? LIGHT_ON_LEVEL : LIGHT_OFF_LEVEL);
  webServer.send(200, F("application/json"), webMeasurementsJson());
}

void setupWebServer() {
  webServer.on(F("/"), HTTP_GET, []() {
    webServer.send_P(200, PSTR("text/html"), WEB_PAGE);
  });
  webServer.on(F("/api/data"), HTTP_GET, []() {
    webServer.send(200, F("application/json"), webMeasurementsJson());
  });
  webServer.on(F("/api/light/on"), HTTP_POST, []() { handleWebLight(true); });
  webServer.on(F("/api/light/off"), HTTP_POST, []() { handleWebLight(false); });
  webServer.onNotFound([]() {
    webServer.send(404, F("text/plain"), F("Not found"));
  });
  webServer.begin();
  Serial.println(F("Web server started on port 80"));
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP                 Show this help"));
  Serial.println(F("  GET                  Read all current values"));
  Serial.println(F("  STATUS               Show health, WiFi and uptime"));
  Serial.println(F("  SET YYYY-MM-DD HH:MM:SS  Set DS3232/DS3231 clock"));
  Serial.println(F("  NTP                  Sync RTC from pool.ntp.org"));
  Serial.println(F("  WIFI                 Connect/reconnect to configured WiFi"));
  Serial.println(F("  SCAN                 Scan the I2C bus"));
  Serial.println(F("  RESET-MINMAX         Reset temperature extremes"));
  Serial.println(F("  JSON                 Print one machine-readable sensor record"));
  Serial.println(F("  PAGE 0|1|2           Select clock, weather or diagnostics page"));
  Serial.println(F("  FORECAST             Show pressure trend and comfort summary"));
  Serial.println(F("  LIGHT ON|OFF         Switch the D0 light output"));
  Serial.println(F("  Web: http://<device-ip>/"));
}

void readSensors() {
  if (!bmeOk) return;
  temperatureC = bme.readTemperature();
  humidity = bme.readHumidity();
  pressureHpa = bme.readPressure() / 100.0F;
  if (isfinite(temperatureC)) {
    minTemperature = min(minTemperature, temperatureC);
    maxTemperature = max(maxTemperature, temperatureC);
  }
}

void updatePressureTrend(uint32_t now) {
  if (!isfinite(pressureHpa)) return;
  if (!isfinite(pressureReferenceHpa) || now - lastPressureTrend >= PRESSURE_TREND_INTERVAL_MS) {
    pressureReferenceHpa = pressureHpa;
    lastPressureTrend = now;
  }
}

void printMeasurements(bool json = false) {
  DateTime now = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  if (json) {
    Serial.print(F("{\"time\":\"")); Serial.print(dateTimeString(now));
    Serial.print(F("\",\"temperature_c\":")); Serial.print(temperatureC, 2);
    Serial.print(F(",\"humidity_pct\":")); Serial.print(humidity, 2);
    Serial.print(F(",\"pressure_hpa\":")); Serial.print(pressureHpa, 2);
    Serial.print(F(",\"dew_point_c\":")); Serial.print(dewPointC(), 2);
    Serial.print(F(",\"heat_index_c\":")); Serial.print(heatIndexC(), 2);
    Serial.print(F(",\"comfort\":\"")); Serial.print(comfortLabel()); Serial.print('"');
    Serial.print(F(",\"pressure_trend\":\"")); Serial.print(pressureTrendLabel()); Serial.print('"');
    Serial.print(F(",\"light_on\":")); Serial.print(lightOn ? F("true") : F("false"));
    Serial.print(F(",\"ip\":\""));
    if (WiFi.status() == WL_CONNECTED) Serial.print(WiFi.localIP());
    else Serial.print(F("disconnected"));
    Serial.print('"');
    Serial.println('}');
    return;
  }
  Serial.print(F("Time: ")); Serial.println(dateTimeString(now));
  Serial.print(F("Temperature: ")); Serial.print(temperatureC, 2); Serial.println(F(" C"));
  Serial.print(F("Humidity: ")); Serial.print(humidity, 2); Serial.println(F(" %"));
  Serial.print(F("Pressure: ")); Serial.print(pressureHpa, 2); Serial.println(F(" hPa"));
  Serial.print(F("Dew point / feels like: ")); Serial.print(dewPointC(), 2);
  Serial.print(F(" / ")); Serial.print(heatIndexC(), 2); Serial.println(F(" C"));
  Serial.print(F("Comfort: ")); Serial.println(comfortLabel());
  Serial.print(F("Pressure trend: ")); Serial.println(pressureTrendLabel());
  Serial.print(F("Light output (D0): ")); Serial.println(lightOn ? F("ON") : F("OFF"));
  Serial.print(F("IP: "));
  if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
  else Serial.println(F("disconnected"));
  Serial.print(F("Temperature min/max: ")); Serial.print(minTemperature, 2);
  Serial.print(F(" / ")); Serial.print(maxTemperature, 2); Serial.println(F(" C"));
}

void scanI2c() {
  Serial.println(F("I2C scan:"));
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  0x"));
      if (address < 16) Serial.print('0');
      Serial.println(address, HEX);
      found++;
    }
    yield();
  }
  if (!found) Serial.println(F("  no devices found"));
  else { Serial.print(found); Serial.println(F(" device(s) found")); }
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print(F("Connecting to WiFi SSID ")); Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
  wifiRequested = true;
}

void setupNtp() {
  if (ntpConfigured) return;
  setenv("TZ", TZ_INFO, 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  ntpConfigured = true;
}

bool pragueDaylightTime(time_t epoch) {
  struct tm utc;
  gmtime_r(&epoch, &utc);
  if (utc.tm_mon < 2 || utc.tm_mon > 9) return utc.tm_mon > 2 && utc.tm_mon < 9;
  if (utc.tm_mon > 2 && utc.tm_mon < 9) return true;

  const uint8_t daysInMarch = 31;
  const uint8_t daysInOctober = 31;
  const uint8_t month = utc.tm_mon + 1;
  const uint8_t daysInMonth = month == 3 ? daysInMarch : daysInOctober;
  DateTime lastSunday(utc.tm_year + 1900, month, daysInMonth, 0, 0, 0);
  const uint8_t transitionDay = daysInMonth - lastSunday.dayOfTheWeek();
  if (month == 3) {
    if (utc.tm_mday != transitionDay) return utc.tm_mday > transitionDay;
    return utc.tm_hour >= 1;
  }
  if (utc.tm_mday != transitionDay) return utc.tm_mday < transitionDay;
  return utc.tm_hour < 1;
}

bool syncRtcFromNtp() {
  setupNtp();
  lastNtpAttempt = millis();
  time_t epoch = time(nullptr);
  if (epoch < 1700000000) return false;
  const time_t pragueEpoch = epoch + (pragueDaylightTime(epoch) ? 7200 : 3600);
  struct tm local;
  gmtime_r(&pragueEpoch, &local);
  rtc.adjust(DateTime(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                      local.tm_hour, local.tm_min, local.tm_sec));
  lastNtpSync = millis();
  return true;
}

bool parseDateTime(const String &input, DateTime &result) {
  int year, month, day, hour, minute, second;
  if (sscanf(input.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day,
             &hour, &minute, &second) != 6) return false;
  if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return false;
  result = DateTime(year, month, day, hour, minute, second);
  return result.isValid();
}

void handleCommand(String command) {
  command.trim();
  if (!command.length()) return;
  String upper = command;
  upper.toUpperCase();
  if (upper == "HELP") printHelp();
  else if (upper == "GET") printMeasurements();
  else if (upper == "JSON") printMeasurements(true);
  else if (upper == "SCAN") scanI2c();
  else if (upper == "WIFI") connectWifi();
  else if (upper == "NTP") {
    connectWifi();
    Serial.println(syncRtcFromNtp() ? F("RTC synchronized from NTP") : F("NTP time not available yet"));
  } else if (upper == "RESET-MINMAX") {
    minTemperature = temperatureC; maxTemperature = temperatureC;
    Serial.println(F("Temperature extremes reset"));
  } else if (upper.startsWith("PAGE ")) {
    int page = upper.substring(5).toInt();
    if (page >= 0 && page <= 2) {
      startupDisplayActive = false;
      displayPage = page;
      Serial.println(F("Display page selected"));
    }
    else Serial.println(F("PAGE must be 0, 1 or 2"));
  } else if (upper == "FORECAST") {
    Serial.print(F("Pressure is ")); Serial.print(pressureTrendLabel());
    Serial.print(F("; comfort is ")); Serial.println(comfortLabel());
    Serial.print(F("Dew point: ")); Serial.print(dewPointC(), 1);
    Serial.print(F(" C, feels like: ")); Serial.print(heatIndexC(), 1); Serial.println(F(" C"));
  } else if (upper == "LIGHT ON" || upper == "LIGHT OFF") {
    lightOn = upper == "LIGHT ON";
    digitalWrite(PIN_LIGHT, lightOn ? LIGHT_ON_LEVEL : LIGHT_OFF_LEVEL);
    Serial.print(F("Light output D0: ")); Serial.println(lightOn ? F("ON") : F("OFF"));
  } else if (upper == "STATUS") {
    Serial.print(F("RTC: ")); Serial.println(rtcOk ? F("OK") : F("missing"));
    Serial.print(F("BME280: ")); Serial.println(bmeOk ? F("OK") : F("missing"));
    Serial.print(F("WiFi: "));
    if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
    else Serial.println(F("disconnected"));
    Serial.print(F("Uptime: ")); Serial.println(uptimeString());
  } else if (upper.startsWith("SET ")) {
    DateTime requested(2000, 1, 1, 0, 0, 0);
    if (!rtcOk || !parseDateTime(command.substring(4), requested)) Serial.println(F("Use: SET YYYY-MM-DD HH:MM:SS"));
    else { rtc.adjust(requested); Serial.print(F("RTC set to ")); Serial.println(dateTimeString(requested)); }
  } else Serial.println(F("Unknown command. Type HELP."));
}

void drawDisplay() {
  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setTextSize(1);
  DateTime now = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  if (displayPage == 0) {
    display.setCursor(0, 0); display.println(F("WEATHER CLOCK"));
    display.setTextSize(2); display.setCursor(0, 12);
    printTwoDigits(display, now.hour()); display.print(':'); printTwoDigits(display, now.minute());
    display.setTextSize(1); display.setCursor(0, 32);
    display.print(now.day()); display.print('/'); display.print(now.month()); display.print('/'); display.println(now.year());
    display.print(F("T ")); display.print(temperatureC, 1); display.print(F("C H ")); display.print(humidity, 0); display.println('%');
  } else if (displayPage == 1) {
    display.setCursor(0, 0); display.println(F("SENSOR DATA"));
    display.print(F("Temp: ")); display.print(temperatureC, 1); display.println(F(" C"));
    display.print(F("Hum : ")); display.print(humidity, 1); display.println(F(" %"));
    display.print(F("Pres: ")); display.print(pressureHpa, 0); display.println(F(" hPa"));
    display.print(F("Dew : ")); display.print(dewPointC(), 1); display.println(F(" C"));
    display.print(F("Feel: ")); display.print(heatIndexC(), 1); display.println(F(" C"));
    display.print(F("Trend ")); display.println(pressureTrendLabel());
  } else {
    display.setCursor(0, 0); display.println(F("DIAGNOSTICS"));
    display.print(F("RTC  ")); display.println(rtcOk ? F("OK") : F("FAIL"));
    display.print(F("BME  ")); display.println(bmeOk ? F("OK") : F("FAIL"));
    display.print(F("WiFi ")); display.println(WiFi.status() == WL_CONNECTED ? F("ON") : F("OFF"));
    display.print(F("D0   ")); display.println(lightOn ? F("ON") : F("OFF"));
    if (WiFi.status() == WL_CONNECTED) display.println(WiFi.localIP());
    else display.println(F("none"));
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(100);
  pinMode(PIN_LIGHT, OUTPUT);
  digitalWrite(PIN_LIGHT, LIGHT_OFF_LEVEL);
  Serial.println(F("\nWemos D1 mini weather clock"));
  Wire.begin(PIN_SDA, PIN_SCL);
  rtcOk = rtc.begin();
  if (rtcOk && rtc.lostPower()) Serial.println(F("RTC lost power; use SET or NTP"));
  bmeOk = bme.begin(I2C_ADDRESS_BME280, &Wire);
  if (!bmeOk) bmeOk = bme.begin(0x77, &Wire);
  display.begin();
  display.setContrast(55);
  display.clearDisplay(); display.setCursor(0, 0); display.println(F("Starting...")); display.display();
  readSensors();
  updatePressureTrend(millis());
  connectWifi();
  setupWebServer();
  printHelp();
  displayPage = 2;
  drawDisplay();
  bootDisplayStart = millis();
  lastDisplayUpdate = bootDisplayStart;
}

void loop() {
  while (Serial.available()) {
    char character = (char) Serial.read();
    if (character == '\n' || character == '\r') { handleCommand(serialLine); serialLine = ""; }
    else if (serialLine.length() < 80) serialLine += character;
  }
  webServer.handleClient();
  uint32_t now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) { lastSensorRead = now; readSensors(); updatePressureTrend(now); }
  if (startupDisplayActive) {
    const uint32_t bootElapsed = now - bootDisplayStart;
    if (bootElapsed >= BOOT_DIAGNOSTICS_MS + BOOT_SENSOR_DATA_MS) {
      startupDisplayActive = false;
      displayPage = 0;
      drawDisplay();
      lastDisplayUpdate = now;
    } else if (bootElapsed >= BOOT_DIAGNOSTICS_MS && displayPage != 1) {
      displayPage = 1;
      drawDisplay();
      lastDisplayUpdate = now;
    }
  } else if (now - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = now;
    displayPage = 0;
    drawDisplay();
  }
  if (wifiRequested && WiFi.status() != WL_CONNECTED && now - lastWifiAttempt > 30000UL) connectWifi();
  if (WiFi.status() == WL_CONNECTED && !ntpConfigured) setupNtp();
  if (WiFi.status() == WL_CONNECTED && ntpConfigured &&
      (lastNtpSync == 0 || now - lastNtpSync >= NTP_SYNC_INTERVAL_MS) &&
      (lastNtpAttempt == 0 || now - lastNtpAttempt >= NTP_RETRY_INTERVAL_MS)) syncRtcFromNtp();
  yield();
}
