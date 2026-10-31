# D1 mini weather clock

PlatformIO firmware for a Wemos D1 mini (ESP8266) with a DS3232-compatible RTC, GYBMEP BMP280/BME280 breakout, Nokia PCD8544 LCD, and a D0 light output.

## Wiring

| Device signal | D1 mini pin | ESP8266 GPIO |
|---|---:|---:|
| I2C SCL | D1 | GPIO5 |
| I2C SDA | D2 | GPIO4 |
| PCD8544 RST | D3 | GPIO0 |
| PCD8544 CE | D4 | GPIO2 |
| PCD8544 DC | D5 | GPIO14 |
| PCD8544 DIN | D6 | GPIO12 |
| PCD8544 CLK | D7 | GPIO13 |
| Light driver input | D0 | GPIO16 |

Connect all grounds together. Use 3.3 V logic for the ESP8266 and LCD. The firmware tries BME280 addresses `0x76` and `0x77`; the RTC is normally `0x68`.

## Build and upload

Install PlatformIO, connect the board, and run:

```text
pio run
pio device list
pio run --target upload       # auto-detects the connected board
pio device monitor            # uses the detected board port
```

The serial monitor uses `115200` baud. `pio device list` shows the available COM ports. If more than one port is present, select the Wemos port explicitly, for example:

```text
pio run --target upload --upload-port COM4
pio device monitor --port COM4
```

PlatformIO normally detects the upload port automatically, so no COM port is hard-coded in `platformio.ini`. If the D1 mini does not appear in `pio device list`, reconnect its USB cable, install the required CH340/CP210x USB-serial driver, and try another data-capable cable or USB port. Before building, copy `src/config.h.example` to `src/config.h` and set `WIFI_SSID`, `WIFI_PASSWORD`, and `TZ_INFO` for your network and timezone. `src/config.h` is ignored by Git and must never be committed; the example contains no real credentials.

At boot, the LCD shows diagnostics for five seconds, sensor data for five seconds, and then remains on the clock screen. The weather page includes dew point, feels-like temperature, and a pressure trend learned over 15 minutes. The light output on D0 starts OFF and is controlled by the `LIGHT ON` and `LIGHT OFF` commands. The diagnostics page shows the Wi-Fi IP address. Temperature minimum and maximum values are kept for the current power session. When Wi-Fi is available, the RTC is refreshed from NTP at boot and every six hours. The built-in web server listens on port 80 when the device is connected to Wi-Fi.

## Web dashboard

Open the IP address printed by the router or discovered with the `STATUS` serial command in a browser, for example `http://192.168.1.42/`. The dashboard refreshes sensor and clock data every five seconds and provides buttons for the D0 light output.

The HTTP API is also available for integrations:

| Method | URL | Action |
|---|---|---|
| `GET` | `/` | Open the dashboard |
| `GET` | `/api/data` | Return current readings as JSON |
| `POST` | `/api/light/on` | Switch D0 HIGH |
| `POST` | `/api/light/off` | Switch D0 LOW |

The web server is only reachable from the local network. Do not expose the ESP8266 directly to the Internet; the endpoints intentionally have no authentication.

## Serial command reference

Commands are case-insensitive and end with Enter:

| Command | Action |
|---|---|
| `HELP` | Print all commands |
| `GET` | Print time and sensor readings |
| `JSON` | Print one machine-readable JSON record |
| `STATUS` | Show RTC/BME/Wi-Fi health and uptime |
| `SET 2026-10-31 20:21:00` | Set the RTC manually |
| `NTP` | Synchronize the RTC from Internet time |
| `WIFI` | Start or retry Wi-Fi connection |
| `SCAN` | Scan and list I2C device addresses |
| `RESET-MINMAX` | Reset session temperature extremes |
| `PAGE 0`, `PAGE 1`, `PAGE 2` | Select clock, weather, or diagnostics page |
| `FORECAST` | Show pressure trend, comfort, dew point, and feels-like temperature |
| `LIGHT ON`, `LIGHT OFF` | Set the D0 light-driver output HIGH or LOW |

`JSON` also includes `dew_point_c`, `heat_index_c`, `comfort`, `pressure_trend`, and `ip`. The pressure trend is only a simple local indicator: rising pressure often suggests clearing weather and falling pressure often suggests unsettled weather. NTP is received in UTC, then converted explicitly to Prague time before it is stored in the RTC: UTC+1 in winter and UTC+2 during European daylight saving time. `TZ_INFO` remains configurable for the system timezone, but the RTC conversion currently targets Central Europe.

`A0` is not used by the firmware because no light sensor is connected. `D0` is a digital output for the light driver and starts OFF after boot. The connected driver is active-low: `LIGHT ON` drives D0 LOW and `LIGHT OFF` drives D0 HIGH. Connect D0 to a transistor/MOSFET or a suitable relay-module input; do not connect a lamp or other high-current load directly to the ESP8266 pin. Use an external supply and common ground, and add a flyback diode for a bare relay coil.

## Troubleshooting

* Run `SCAN` first. Expected addresses are `0x68` (RTC) and `0x76` or `0x77` (BME280).
* If the LCD is blank, check `CE` and `RST`, common ground, and the contrast value in `setup()`.
* If Wi-Fi is unavailable, local sensor and RTC features continue working; use `SET` for a manual clock.
* A DS3232 and DS3231 use the same address and are supported by `RTC_DS3231` in RTClib for these clock operations.
