# Arduino UNO Weather Clock

Sketch for an Arduino UNO, a 2004A LCD with an I2C backpack, a GY-68 BMP180, a ZS-042 RTC (DS3231), and a capacitive touch module (typically TTP223).

## Wiring

All modules share one I2C bus:

| Arduino UNO | LCD 2004A | GY-68 BMP180 | ZS-042 |
|---|---|---|---|
| `5V` | `VCC` | `VCC`* | `VCC`* |
| `GND` | `GND` | `GND` | `GND` |
| `A4 (SDA)` | `SDA` | `SDA` | `SDA` |
| `A5 (SCL)` | `SCL` | `SCL` | `SCL` |
| `D2` | — | — | capacitive module `OUT` |

### Wiring diagram

```text
                         +-- LCD 2004A I2C backpack
                         |      VCC -> 5V
Arduino UNO              |      GND -> GND
  5V  -------------------+      SDA -> A4/SDA
   |                     |      SCL -> A5/SCL
   +-- GY-68 BMP180      |
   |      VCC -> 5V*     +-- ZS-042 RTC
   |      GND -> GND            VCC -> 5V
   |      SDA -----------+      GND -> GND
   |      SCL -----------+      SDA -> A4/SDA
   |                             SCL -> A5/SCL
  GND -------------------------- GND (common ground)

  D2  <-------------------------- OUT capacitive module (TTP223)
  5V  --------------------------- VCC capacitive module
  GND --------------------------- GND capacitive module
```

`A4/SDA` and `A5/SCL` are shared signals: all three I2C modules are connected in parallel, not in series. Do not connect the `SDA` or `SCL` wires to `D2`.

Connect the capacitive module as a normal digital input: `VCC` to `5V`, `GND` to `GND`, and `OUT` to `D2`. This wiring assumes a typical `TTP223` module with an active-`HIGH` output. If you have a different sensor or an active-`LOW` output, adjust `TOUCH_PIN` and the input logic in the sketch.

Touching the module toggles the LCD backlight. The LCD backpack should not be used as an input for the touch sensor: its I2C expander controls the LCD, and directly connecting the `OUT` signal to its pins can cause a conflict. The firmware therefore controls the backlight through the same library as the LCD. An external LED may only be connected through a suitable resistor and a separate output, never directly to the I2C `SDA/SCL` lines.

### Why the UNO has both `A4/A5` and `SDA/SCL`

On an Arduino UNO R3, the top `SDA` and `SCL` connectors carry the same signals as `A4` and `A5`: `SDA = A4`, `SCL = A5`. They are not additional GPIO pins or two separate buses. Use either connector pair and do not bridge the pairs together; all I2C modules connect in parallel to the same `SDA`, `SCL`, `5V`, and `GND`.

`*` Check the required supply voltage for your specific BMP180 board. Some GY-68 boards include a regulator and accept 5V, while a bare BMP180 requires 3.3V. If the module has no level shifting, use 3.3V and an I2C level shifter. The ZS-042 RTC is commonly powered from 5V.

Typical I2C addresses are LCD `0x27` (sometimes `0x3F`), BMP180 `0x77`, and DS3231 `0x68`. If the LCD uses `0x3F`, change `LCD_ADDRESS` in `clock.ino`.

## Installation and upload

In the Arduino IDE, install these libraries:

- `RTClib` od Adafruit
- `Adafruit BMP085 Library` (obsahuje podporu BMP180)
- `LiquidCrystal_I2C` (varianta s API `init()`)

Select **Arduino UNO**, upload `clock.ino`, and open the Serial Monitor at `9600 baud`, preferably with the line ending set to `Newline` or `Both NL & CR`.

## Serial commands

```text
HELP
SCAN
STATUS
READ
REPORT ON
REPORT OFF
BACKLIGHT ON
BACKLIGHT OFF
TOUCH ON
TOUCH OFF
SETTIME 2026-10-25 22:34:00
SYNC 2026-10-25 22:34:00
```

Both `SETTIME` and `SYNC` set the RTC. The firmware cannot obtain the computer's time automatically, so the user or a host program must send a timestamp in the format shown above. `SCAN` lists all responding I2C addresses, allowing you to verify the actual module addresses without uploading the sketch again.

The LCD displays the time, date, temperature, and pressure simultaneously. When `REPORT ON` is enabled, readings are printed to Serial every 10 seconds.

`BACKLIGHT ON/OFF` manually controls the LCD backlight. `TOUCH ON/OFF` enables or disables automatic backlight toggling through touch. Pin `D2` is not part of the I2C bus; `A4/A5` remain dedicated to `SDA/SCL`.

The LCD uses one fixed view. The rows do not rotate, and the firmware does not use `lcd.clear()`, so the display does not flicker:

```text
Arduino Weather       (or custom text)
Time: HH:MM:SS YYYY-MM-DD
Temp: xx.x C
Press: xxxx.x hPa
```

The firmware updates only rows whose values have changed. Device diagnostics remain available through the `STATUS` Serial command.

### Custom first row

The first row can be changed through Serial (text is truncated to 20 characters):

```text
LCDLINE FIRST Ahoj Arduino!
LCDLINE FIRST
```

An empty command restores the first row to `Arduino Weather`. Device status and diagnostic information are shown in the Serial Monitor after the `STATUS` command; the LCD remains reserved for time, temperature, and pressure.

If Serial confirms `LCD backlight ON` but the LED does not physically light up, the command is working only at the I2C backpack level. Check the contrast trimmer, `VCC/GND` power, the soldered `LED/BL` jumper on the backpack, and the LCD backlight itself; some modules use a jumper-only backlight circuit or have a faulty transistor. `BACKLIGHT ON` cannot repair an open LED circuit.