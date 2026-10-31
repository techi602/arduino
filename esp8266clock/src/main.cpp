#include <Arduino.h>
#include "config.h"
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <time.h>
#include <ArduinoJson.h>
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
constexpr uint32_t BOOT_DIAGNOSTICS_MS = 15000;
constexpr uint32_t BOOT_SENSOR_DATA_MS = 5000;
constexpr uint32_t BOOT_DISPLAY_REFRESH_MS = 1000;
constexpr uint32_t PRESSURE_TREND_INTERVAL_MS = 900000;
constexpr uint32_t NTP_SYNC_INTERVAL_MS = 21600000;
constexpr uint32_t NTP_RETRY_INTERVAL_MS = 30000;
constexpr uint32_t INTERNET_WEATHER_INTERVAL_MS = 900000;
constexpr uint32_t INTERNET_WEATHER_STALE_MS = 1800000;
constexpr uint8_t I2C_ADDRESS_BME280 = 0x76;
constexpr uint16_t WEB_SERVER_PORT = 80;
constexpr char MDNS_HOSTNAME[] = "weather-clock";

Adafruit_BME280 bme;
RTC_DS3231 rtc;
Adafruit_PCD8544 display(PIN_LCD_CLK, PIN_LCD_DIN, PIN_LCD_DC, PIN_LCD_CE, PIN_LCD_RST);
ESP8266WebServer webServer(WEB_SERVER_PORT);

bool rtcOk = false;
bool bmeOk = false;
bool wifiRequested = false;
bool mdnsStarted = false;
bool ntpConfigured = false;
bool lightOn = false;
uint8_t displayPage = 0;
uint8_t clockInfoPage = 0;
uint32_t lastSensorRead = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t lastWifiAttempt = 0;
uint32_t lastNtpSync = 0;
uint32_t lastNtpAttempt = 0;
uint32_t lastPressureTrend = 0;
uint32_t lastInternetWeather = 0;
uint32_t lastInternetWeatherSuccess = 0;
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
<h2>Internet · <span id="remoteLocation">--</span></h2><div class="grid"><div class="card">Remote weather<div class="value" id="remoteWeather">--</div></div>
<div class="card">Remote temperature<div class="value"><span id="remoteTemperature">--</span> °C</div></div>
<div class="card">Remote rain<div class="value"><span id="remoteRain">--</span> mm</div></div>
<div class="card">Remote wind<div class="value"><span id="remoteWind">--</span> km/h</div></div>
<div class="card">Sunrise<div class="value" id="sunrise">--</div></div>
<div class="card">Sunset<div class="value" id="sunset">--</div></div></div>
<div class="buttons"><button id="on" onclick="setLight('on')">Light ON</button><button id="off" onclick="setLight('off')">Light OFF</button></div>
<p>Light output: <strong id="lightState">--</strong> (D0)</p><p><small>Pressure trend: <span id="trend">--</span> · Uptime: <span id="uptime">--</span></small></p>
</main><script>
const $=id=>document.getElementById(id);function show(v){$('time').textContent=v.time;$('temperature').textContent=Number(v.temperature_c).toFixed(1);$('humidity').textContent=Number(v.humidity_pct).toFixed(1);$('pressure').textContent=Number(v.pressure_hpa).toFixed(1);$('dew').textContent=Number(v.dew_point_c).toFixed(1);$('feels').textContent=Number(v.heat_index_c).toFixed(1);$('comfort').textContent=v.comfort;$('remoteLocation').textContent=v.internet_location;$('remoteWeather').textContent=v.internet_weather;$('remoteTemperature').textContent=v.internet_weather_ok?Number(v.internet_temperature_c).toFixed(1):'--';$('remoteRain').textContent=v.internet_weather_ok?Number(v.internet_rain_mm).toFixed(1):'--';$('remoteWind').textContent=v.internet_weather_ok?Number(v.internet_wind_kmh).toFixed(1):'--';$('sunrise').textContent=v.sunrise||'--';$('sunset').textContent=v.sunset||'--';$('lightState').textContent=v.light_on?'ON':'OFF';$('trend').textContent=v.pressure_trend;$('uptime').textContent=v.uptime;}
async function refresh(){try{const r=await fetch('/api/data');if(!r.ok)throw Error();show(await r.json());$('message').textContent='Live data · refreshed '+new Date().toLocaleTimeString()}catch(e){$('message').textContent='Device data unavailable'}}
async function setLight(state){try{const r=await fetch('/api/light/'+state,{method:'POST'});const v=await r.json();show(v);$('message').textContent='Light switched '+state.toUpperCase()}catch(e){$('message').textContent='Could not switch light'}}refresh();setInterval(refresh,5000);
</script></body></html>)HTML";

float temperatureC = NAN;
float humidity = NAN;
float pressureHpa = NAN;
float pressureReferenceHpa = NAN;
float minTemperature = INFINITY;
float maxTemperature = -INFINITY;
float internetTemperatureC = NAN;
float internetHumidity = NAN;
float internetApparentC = NAN;
float internetRainMm = NAN;
float internetWindKmh = NAN;
int internetWeatherCode = -1;
bool internetWeatherOk = false;
String sunriseTime;
String sunsetTime;

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

const __FlashStringHelper *internetWeatherLabel() {
  switch (internetWeatherCode) {
    case 0: return F("clear");
    case 1: case 2: return F("partly cloudy");
    case 3: return F("overcast");
    case 45: case 48: return F("fog");
    case 51: case 53: case 55: case 56: case 57: return F("drizzle");
    case 61: case 63: case 65: case 66: case 67: return F("rain");
    case 71: case 73: case 75: case 77: return F("snow");
    case 80: case 81: case 82: return F("showers");
    case 85: case 86: return F("snow showers");
    case 95: case 96: case 99: return F("thunderstorm");
    default: return F("unknown");
  }
}

void drawWeatherIcon(int16_t x, int16_t y) {
  if (!internetWeatherOk || internetWeatherCode < 0) {
    display.drawRect(x + 2, y + 2, 12, 12, BLACK);
    display.drawLine(x + 5, y + 5, x + 10, y + 10, BLACK);
    display.drawLine(x + 10, y + 5, x + 5, y + 10, BLACK);
    return;
  }

  const int code = internetWeatherCode;
  if (code == 0) {
    display.drawCircle(x + 8, y + 8, 4, BLACK);
    for (uint8_t angle = 0; angle < 8; angle++) {
      const int16_t dx = angle % 2 == 0 ? 0 : (angle < 4 ? 6 : -6);
      const int16_t dy = angle % 2 == 0 ? (angle == 0 ? -7 : angle == 2 ? 7 : angle == 4 ? 0 : 0) : 0;
      if (angle % 2 == 0) display.drawLine(x + 8, y + 8 + dy, x + 8, y + 8 + dy + (dy < 0 ? -2 : 2), BLACK);
      else display.drawLine(x + 8 + dx, y + 8, x + 8 + dx + (dx < 0 ? -2 : 2), y + 8, BLACK);
    }
    return;
  }

  const bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  const bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
  const bool storm = code >= 95;
  const bool fog = code == 45 || code == 48;
  const bool cloud = code == 1 || code == 2 || code == 3 || rain || snow || storm;

  if (cloud) {
    display.fillCircle(x + 6, y + 8, 4, BLACK);
    display.fillCircle(x + 11, y + 7, 3, BLACK);
    display.fillRect(x + 4, y + 8, 10, 5, BLACK);
    display.drawPixel(x + 3, y + 12, WHITE);
  }
  if (fog) {
    display.drawLine(x + 2, y + 6, x + 14, y + 6, BLACK);
    display.drawLine(x + 1, y + 9, x + 13, y + 9, BLACK);
    display.drawLine(x + 3, y + 12, x + 15, y + 12, BLACK);
    return;
  }
  if (storm) {
    display.fillTriangle(x + 9, y + 9, x + 5, y + 15, x + 9, y + 13, BLACK);
    display.fillTriangle(x + 9, y + 13, x + 13, y + 7, x + 9, y + 9, BLACK);
  } else if (snow) {
    display.drawLine(x + 5, y + 14, x + 5, y + 16, BLACK);
    display.drawLine(x + 10, y + 14, x + 10, y + 16, BLACK);
    display.drawPixel(x + 5, y + 15, WHITE);
    display.drawPixel(x + 10, y + 15, WHITE);
  } else if (rain) {
    display.drawLine(x + 6, y + 14, x + 4, y + 17, BLACK);
    display.drawLine(x + 11, y + 14, x + 9, y + 17, BLACK);
  } else if (!cloud) {
    display.drawRect(x + 2, y + 2, 12, 12, BLACK);
  }
}

void drawHumidityIcon(int16_t x, int16_t y) {
  display.fillTriangle(x + 3, y, x, y + 4, x + 6, y + 4, BLACK);
  display.fillCircle(x + 3, y + 5, 3, BLACK);
  display.drawPixel(x + 2, y + 3, WHITE);
}

void drawDegreeSymbol(int16_t x, int16_t y) {
  display.drawCircle(x + 1, y + 1, 1, BLACK);
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

void appendJsonNumber(String &json, float value, uint8_t decimals) {
  if (isfinite(value)) json += String(value, decimals);
  else json += F("null");
}

void expireStaleInternetWeather() {
  if (internetWeatherOk && lastInternetWeatherSuccess != 0 &&
      millis() - lastInternetWeatherSuccess >= INTERNET_WEATHER_STALE_MS) {
    internetWeatherOk = false;
  }
}

String webMeasurementsJson() {
  DateTime now = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  String json = F("{\"time\":\"");
  json += dateTimeString(now);
  json += F("\",\"temperature_c\":"); appendJsonNumber(json, temperatureC, 2);
  json += F(" ,\"humidity_pct\":"); appendJsonNumber(json, humidity, 2);
  json += F(" ,\"pressure_hpa\":"); appendJsonNumber(json, pressureHpa, 2);
  json += F(" ,\"dew_point_c\":"); appendJsonNumber(json, dewPointC(), 2);
  json += F(" ,\"heat_index_c\":"); appendJsonNumber(json, heatIndexC(), 2);
  json += F(" ,\"comfort\":\""); json += comfortLabel(); json += F("\"");
  json += F(" ,\"pressure_trend\":\""); json += pressureTrendLabel(); json += F("\"");
  json += F(" ,\"internet_location\":\""); json += WEATHER_LOCATION; json += F("\"");
  json += F(" ,\"internet_weather_ok\":"); json += internetWeatherOk ? F("true") : F("false");
  json += F(" ,\"internet_weather\":\""); json += internetWeatherLabel(); json += F("\"");
  json += F(" ,\"internet_temperature_c\":"); appendJsonNumber(json, internetWeatherOk ? internetTemperatureC : NAN, 2);
  json += F(" ,\"internet_humidity_pct\":"); appendJsonNumber(json, internetWeatherOk ? internetHumidity : NAN, 1);
  json += F(" ,\"internet_apparent_c\":"); appendJsonNumber(json, internetWeatherOk ? internetApparentC : NAN, 2);
  json += F(" ,\"internet_rain_mm\":"); appendJsonNumber(json, internetWeatherOk ? internetRainMm : NAN, 1);
  json += F(" ,\"internet_wind_kmh\":"); appendJsonNumber(json, internetWeatherOk ? internetWindKmh : NAN, 1);
  json += F(" ,\"sunrise\":\""); json += sunriseTime; json += F("\"");
  json += F(" ,\"sunset\":\""); json += sunsetTime; json += F("\"");
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
  Serial.println(F("  WEATHER              Refresh internet weather now"));
  Serial.println(F("  LIGHT ON|OFF         Switch the D0 light output"));
  Serial.println(F("  Web: http://<device-ip>/"));
  Serial.println(F("  mDNS: http://weather-clock.local/"));
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

void fetchInternetWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  lastInternetWeather = millis();
  WiFiClient client;
  HTTPClient http;
  String url = F("http://api.open-meteo.com/v1/forecast?latitude=");
  url += String(WEATHER_LATITUDE, 4);
  url += F("&longitude="); url += String(WEATHER_LONGITUDE, 4);
  url += F("&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,precipitation,wind_speed_10m&daily=sunrise,sunset&forecast_days=1&timezone=auto");
  http.setTimeout(5000);
  if (!http.begin(client, url)) return;
  const int status = http.GET();
  if (status == HTTP_CODE_OK) {
    DynamicJsonDocument document(1536);
    const DeserializationError error = deserializeJson(document, http.getString());
    if (!error) {
      JsonObject current = document["current"];
      internetTemperatureC = current["temperature_2m"] | NAN;
      internetHumidity = current["relative_humidity_2m"] | NAN;
      internetApparentC = current["apparent_temperature"] | NAN;
      internetRainMm = current["precipitation"] | NAN;
      internetWindKmh = current["wind_speed_10m"] | NAN;
      internetWeatherCode = current["weather_code"] | -1;
      internetWeatherOk = isfinite(internetTemperatureC);
      if (internetWeatherOk) lastInternetWeatherSuccess = millis();
      JsonArray sunrise = document["daily"]["sunrise"];
      JsonArray sunset = document["daily"]["sunset"];
      if (!sunrise.isNull() && sunrise.size() > 0) sunriseTime = sunrise[0].as<const char *>();
      if (!sunset.isNull() && sunset.size() > 0) sunsetTime = sunset[0].as<const char *>();
      if (sunriseTime.length() >= 16) sunriseTime = sunriseTime.substring(11, 16);
      if (sunsetTime.length() >= 16) sunsetTime = sunsetTime.substring(11, 16);
    }
  }
  http.end();
  expireStaleInternetWeather();
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
    Serial.println(webMeasurementsJson());
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
  Serial.print(F("Internet weather (")); Serial.print(WEATHER_LOCATION); Serial.print(F("): "));
  if (internetWeatherOk) {
    Serial.print(internetWeatherLabel()); Serial.print(F(", ")); Serial.print(internetTemperatureC, 1);
    Serial.print(F(" C, rain ")); Serial.print(internetRainMm, 1);
    Serial.print(F(" mm, wind ")); Serial.print(internetWindKmh, 1); Serial.println(F(" km/h"));
  } else Serial.println(F("unavailable"));
  Serial.print(F("Sunrise / sunset: ")); Serial.print(sunriseTime.length() ? sunriseTime : F("--"));
  Serial.print(F(" / ")); Serial.println(sunsetTime.length() ? sunsetTime : F("--"));
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

void setupMdns() {
  if (mdnsStarted || WiFi.status() != WL_CONNECTED) return;
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    mdnsStarted = true;
    Serial.print(F("mDNS: http://")); Serial.print(MDNS_HOSTNAME); Serial.println(F(".local/"));
  } else {
    Serial.println(F("mDNS setup failed"));
  }
}

void setupNtp() {
  if (ntpConfigured) return;
  setenv("TZ", TZ_INFO, 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  ntpConfigured = true;
}

bool syncRtcFromNtp() {
  setupNtp();
  lastNtpAttempt = millis();
  time_t epoch = time(nullptr);
  if (epoch < 1700000000) return false;
  struct tm local;
  localtime_r(&epoch, &local);
  rtc.adjust(DateTime(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                      local.tm_hour, local.tm_min, local.tm_sec));
  lastNtpSync = millis();
  return true;
}

bool parseDateTime(const String &input, DateTime &result) {
  int year, month, day, hour, minute, second;
  if (sscanf(input.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day,
             &hour, &minute, &second) != 6) return false;
  const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  const int maximumDay = month == 2 && leapYear ? 29 : (month >= 1 && month <= 12 ? daysInMonth[month - 1] : 0);
  if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > maximumDay ||
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
  } else if (upper == "WEATHER") {
    fetchInternetWeather();
    Serial.print(F("Internet weather: ")); Serial.println(internetWeatherOk ? F("updated") : F("unavailable"));
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
    Serial.print(F("mDNS: ")); Serial.println(mdnsStarted ? F("weather-clock.local") : F("unavailable"));
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
    display.setCursor(0, 0);
    if (clockInfoPage == 0) {
      display.println(F("WEATHER CLOCK"));
    } else if (clockInfoPage == 1) {
      display.print(F("RAIN "));
      if (internetWeatherOk) display.print(internetRainMm, 1); else display.print(F("--"));
      display.println(F(" mm"));
    } else if (clockInfoPage == 2) {
      display.print(F("OUT TEMP "));
      if (internetWeatherOk) display.print(internetTemperatureC, 1); else display.print(F("--"));
      display.println(F(" C"));
    } else if (clockInfoPage == 3) {
      display.print(F("WIND "));
      if (internetWeatherOk) display.print(internetWindKmh, 1); else display.print(F("--"));
      display.println(F(" km/h"));
    } else if (clockInfoPage == 4) {
      display.print(F("SUNRISE ")); display.println(sunriseTime.length() ? sunriseTime : F("--"));
    } else {
      display.print(F("SUNSET ")); display.println(sunsetTime.length() ? sunsetTime : F("--"));
    }
    display.setTextSize(2); display.setCursor(0, 12);
    printTwoDigits(display, now.hour()); display.print(':'); printTwoDigits(display, now.minute());
    drawWeatherIcon(64, 12);
    display.setTextSize(1); display.setCursor(0, 32);
    display.print(now.day()); display.print('/'); display.print(now.month()); display.print('/'); display.println(now.year());
    display.setCursor(0, 40); display.print(temperatureC, 1);
    const int16_t degreeX = display.getCursorX();
    drawDegreeSymbol(degreeX, 40);
    display.setCursor(degreeX + 4, 40); display.print('C');
    drawHumidityIcon(48, 40); display.setCursor(58, 40); display.print(humidity, 0); display.print('%');
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
    display.print(F("RTC ")); display.print(rtcOk ? F("OK") : F("FAIL"));
    display.print(F(" BME ")); display.println(bmeOk ? F("OK") : F("FAIL"));
    display.print(F("WiFi "));
    if (WiFi.status() == WL_CONNECTED) display.println(F("CONNECTED"));
    else if (wifiRequested) display.println(F("WAITING"));
    else display.println(F("OFF"));
    display.print(F("D0   ")); display.println(lightOn ? F("ON") : F("OFF"));
    if (WiFi.status() == WL_CONNECTED) display.println(WiFi.localIP());
    else display.println(F("waiting for IP"));
    display.print(F("Internet ")); display.println(internetWeatherOk ? F("OK") : F("waiting"));
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  delay(100);
  pinMode(PIN_LIGHT, OUTPUT);
  lightOn = true;
  digitalWrite(PIN_LIGHT, LIGHT_ON_LEVEL);
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
  fetchInternetWeather();
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
  expireStaleInternetWeather();
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
    } else if (displayPage == 2 && now - lastDisplayUpdate >= BOOT_DISPLAY_REFRESH_MS) {
      lastDisplayUpdate = now;
      drawDisplay();
    }
  } else if (now - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = now;
    displayPage = 0;
    clockInfoPage = (clockInfoPage + 1) % 6;
    drawDisplay();
  }
  if (wifiRequested && WiFi.status() != WL_CONNECTED && now - lastWifiAttempt > 30000UL) connectWifi();
  if (WiFi.status() == WL_CONNECTED && !ntpConfigured) setupNtp();
  if (WiFi.status() == WL_CONNECTED) setupMdns();
  if (mdnsStarted) MDNS.update();
  if (WiFi.status() == WL_CONNECTED && (lastInternetWeather == 0 || now - lastInternetWeather >= INTERNET_WEATHER_INTERVAL_MS)) {
    fetchInternetWeather();
  }
  if (WiFi.status() == WL_CONNECTED && ntpConfigured &&
      (lastNtpSync == 0 || now - lastNtpSync >= NTP_SYNC_INTERVAL_MS) &&
      (lastNtpAttempt == 0 || now - lastNtpAttempt >= NTP_RETRY_INTERVAL_MS)) syncRtcFromNtp();
  yield();
}
