# Arduino UNO Weather Clock

Sketch pro Arduino UNO, LCD 2004A s I2C backpackem, GY-68 BMP180, ZS-042 RTC (DS3231) a kapacitní dotykový modul (typicky TTP223).

## Wiring

Všechny moduly sdílejí jednu I2C sběrnici:

| Arduino UNO | LCD 2004A | GY-68 BMP180 | ZS-042 |
|---|---|---|---|
| `5V` | `VCC` | `VCC`* | `VCC`* |
| `GND` | `GND` | `GND` | `GND` |
| `A4 (SDA)` | `SDA` | `SDA` | `SDA` |
| `A5 (SCL)` | `SCL` | `SCL` | `SCL` |
| `D2` | — | — | kapacitní modul `OUT` |

### Schéma zapojení

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
  GND -------------------------- GND (společná zem)

  D2  <-------------------------- OUT kapacitního modulu (TTP223)
  5V  --------------------------- VCC kapacitního modulu
  GND --------------------------- GND kapacitního modulu
```

`A4/SDA` a `A5/SCL` jsou společné vodiče: všechny tři I2C moduly se připojují paralelně, nikoli za sebou. Vodiče `SDA` a `SCL` nepřipojujte na `D2`.

Kapacitní modul zapojte jako běžný digitální vstup: `VCC` na `5V`, `GND` na `GND` a `OUT` na `D2`. Toto zapojení předpokládá běžný modul `TTP223` s aktivním výstupem `HIGH`. Pokud máte jiný senzor nebo aktivní `LOW` výstup, je nutné upravit `TOUCH_PIN`/logiku ve sketchi.

Dotyk na modulu přepíná podsvit LCD. LCD backpack není vhodné použít jako vstup pro dotykový senzor: jeho I2C expander ovládá LCD a přímé spojení výstupu `OUT` s jeho piny může způsobit konflikt. Podsvit proto řídí firmware přes stejnou knihovnu jako dosud. Externí LED lze připojit pouze přes vhodný rezistor a samostatný výstup, ne přímo na I2C `SDA/SCL`.

### Proč jsou na UNO současně `A4/A5` a `SDA/SCL`

Na Arduino UNO R3 jsou horní konektory `SDA` a `SCL` elektricky vyvedením stejných signálů jako `A4` a `A5`: `SDA = A4`, `SCL = A5`. Nejde o další GPIO ani o dvě oddělené sběrnice. Použijte jednu dvojici, nepropojujte je mezi sebou; všechny I2C moduly se připojují paralelně na stejné `SDA`, `SCL`, `5V` a `GND`.

`*` U BMP180 ověřte napájecí napětí konkrétní destičky. Některé GY-68 mají regulátor a 5V vstup, holý BMP180 vyžaduje 3.3V. Pokud modul nemá převod úrovní, použijte 3.3V a level shifter pro I2C. RTC ZS-042 se běžně napájí 5V.

Typické I2C adresy jsou LCD `0x27` (někdy `0x3F`), BMP180 `0x77` a DS3231 `0x68`. Pokud LCD používá `0x3F`, změňte `LCD_ADDRESS` v `clock.ino`.

## Instalace a nahrání

V Arduino IDE nainstalujte knihovny:

- `RTClib` od Adafruit
- `Adafruit BMP085 Library` (obsahuje podporu BMP180)
- `LiquidCrystal_I2C` (varianta s API `init()`)

Vyberte **Arduino UNO**, nahrajte `clock.ino` a otevřete Serial Monitor na `9600 baud`, ideálně s koncem řádku `Newline` nebo `Both NL & CR`.

## Serial příkazy

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

`SETTIME` i `SYNC` nastaví RTC. Firmware čas z počítače sám nezíská; hostitelský program nebo uživatel proto musí poslat timestamp v uvedeném formátu. `SCAN` vypíše všechny odpovídající I2C adresy, takže lze ověřit skutečné adresy modulů bez nového nahrávání sketchu.

LCD zobrazuje čas, datum, teplotu a tlak současně. Hodnoty se při `REPORT ON` vypisují na Serial každých 10 sekund.

`BACKLIGHT ON/OFF` ručně nastaví podsvit LCD. `TOUCH ON/OFF` zapíná nebo vypíná automatické přepínání podsvitu dotykem. Pin `D2` není součástí I2C sběrnice; `A4/A5` zůstávají vyhrazené pro `SDA/SCL`.

LCD používá jeden pevný pohled. Řádky se nerotují a firmware nepoužívá `lcd.clear()`, takže displej nebliká:

```text
Arduino Weather       (nebo vlastní text)
Time: HH:MM:SS YYYY-MM-DD
Temp: xx.x C
Press: xxxx.x hPa
```

Firmware aktualizuje jen řádek, jehož hodnota se změnila. Diagnostika zařízení zůstává dostupná příkazem `STATUS` přes Serial.

### Vlastní první řádek

První řádek lze změnit přes Serial (text se zkrátí na 20 znaků):

```text
LCDLINE FIRST Ahoj Arduino!
LCDLINE FIRST
```

Prázdný příkaz vrátí první řádek k textu `Arduino Weather`. Stav zařízení a diagnostické informace se zobrazí v Serial Monitoru po příkazu `STATUS`; LCD zůstává vyhrazené pro čas, teplotu a tlak.

Pokud Serial potvrdí `LCD backlight ON`, ale LED fyzicky nesvítí, příkaz funguje pouze na úrovni I2C backpacku. Zkontrolujte trimr kontrastu, napájení `VCC/GND`, pájený jumper `LED/BL` na backpacku a samotné LED podsvícení LCD; některé moduly mají podsvit řešený pouze jumperem nebo vadný tranzistor. `BACKLIGHT ON` neopraví přerušený obvod LED.