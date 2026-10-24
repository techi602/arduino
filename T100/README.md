# T100 Hardware Documentation

This document is an English summary of the original JPEG documentation stored in this directory. The images appear to describe the DOIT T100 tracked robot chassis and its 25 mm geared motors.

## Hardware overview

The platform is a two-track robot chassis with:

- Two DC geared motors, one for each track.
- Metal side rails and track wheels.
- Rubber/plastic continuous tracks.
- A battery holder mounted on the chassis.
- A controller or motor-driver board mounted above the chassis.
- Optional Hall-effect encoder outputs from the motors.

The product photographs show several compatible controller-board arrangements. They should be treated as assembly references, not as a guaranteed schematic for this particular robot.

## Motor specification

The motor documentation identifies the motor as a 25 mm gearmotor with a nominal working voltage of **9 V**.

| Parameter | Stated value |
| --- | ---: |
| Working voltage | 9 V DC |
| Output speed | 150 ± 10 rpm |
| Rated speed | 100 ± 10 rpm |
| No-load current | 200 mA maximum |
| Rated current | 1,200 mA maximum |
| Stall current | 4,500 mA maximum |
| Rated torque | 3,000 g·cm |
| Stall torque | 9.5 kg·cm |
| Noise | 56 dB |
| Encoder | Hall sensor, 2 pulses/revolution |
| Motor diameter | 25 mm |
| Output shaft length | 14.5 mm |
| Shaft diameter | 4 mm at the output, with a 3.5 mm section shown in the drawing |
| Shaft end play | 0.05–0.50 mm |
| Mounting screw | M3.0 |

The separate motor test sheet also lists approximately `11500 ±10 rpm` and `180 mA` for the bare motor, before the gearbox. The geared-output values are the useful values for robot drive calculations.

### Important current implication

One motor can draw up to approximately `4.5 A` when stalled. Two motors can therefore demand roughly `9 A` during a simultaneous stall, in addition to startup and wiring losses. The motor driver, battery, connectors, fuse, and wiring must all tolerate the expected peak current.

Do not size the power system from the `200 mA` no-load value alone. A tracked robot can approach stall current when a track is blocked, the robot is pushing against an obstacle, or it starts on a high-friction surface.

## Power architecture

The motors are 9 V motors, while the Wemos ESP8266 requires a regulated low-voltage supply. Use separate power branches from the same battery rather than powering the Wemos directly from the motor supply:

```text
Battery
  |
  +-- fuse and switch -- motor driver -- left and right 9 V motors
  |
  +-- buck converter -- regulated supply -- Wemos ESP8266
                              |
                              +-- common GND with the motor driver
```

Recommendations:

- Select a battery whose voltage is suitable for a **9 V motor system**. Check the fully charged voltage, not only the nominal label.
- Use a buck converter for the Wemos input and set its output to the voltage expected by the board input.
- Do not connect the battery directly to the Wemos `3V3` pin.
- Connect logic ground and motor-driver ground together at a low-impedance point.
- Keep motor-current wiring short and thicker than signal wiring.
- Add bulk capacitance close to the motor driver, such as a suitably rated `470–1000 µF` capacitor, and follow the driver manufacturer's recommendations.
- Add a fuse and a physical motor power switch. The software `POWER_PIN` is not a substitute for a real emergency disconnect.

The exact battery choice depends on the motor driver's allowable voltage and the required runtime. A nominal 9 V battery is not automatically safe: some battery types have a substantially higher voltage when fully charged, while rectangular PP3-style 9 V batteries usually cannot supply the required motor current.

## Controller and motor-driver wiring

The current firmware configuration defines the following motor-control signals:

| Function | ESP8266 default | ESP32 default |
| --- | --- | --- |
| Left motor PWM | `D6` | GPIO `25` |
| Right motor PWM | `D4` | GPIO `26` |
| Left motor direction | `D7` | GPIO `27` |
| Right motor direction | `D5` | GPIO `14` |
| Motor-driver enable/power | `D8` | GPIO `33` |

These values come from `config.h` and must match the actual shield or driver board. The JPEG product photographs show more than one board layout, so verify the silkscreen and continuity with a multimeter before applying motor power.

The firmware drives motor speed with PWM and uses separate direction signals. It assumes that the driver enable signal is active-high. If the installed board uses the opposite polarity, change the configuration and test with the tracks lifted from the ground.

## Encoder notes

The motor images mention a Hall encoder with two pulses per revolution. The current firmware does not use encoder feedback; it operates open-loop. Consequently:

- A commanded speed is a PWM request, not a measured RPM.
- Left and right tracks may move at different actual speeds.
- Battery voltage, load, friction, and track tension affect movement.
- Encoder integration would enable measured speed control, stall detection, and odometry.

Before using the encoder, identify the encoder supply voltage, output type, connector pinout, and whether the stated pulse count refers to the motor shaft or the gearbox output shaft.

## Mechanical assembly observations

The JPEGs show:

- A two-track chassis with a motor on each side.
- Gold-colored drive sprockets and idler wheels in some versions.
- M3 fasteners and coupling hardware.
- A battery holder located near the center of the chassis.
- Several possible controller mounting positions.

Check that both tracks turn freely before connecting power. Tracks that are too tight increase current and gearbox load; tracks that are too loose can skip teeth or come off the sprocket.

## Safe first-power procedure

1. Keep the robot on a stand with both tracks off the ground.
2. Disconnect motor power while checking the Wemos and driver logic supply.
3. Verify battery polarity and measure the battery voltage with a multimeter.
4. Verify the buck-converter output before connecting the Wemos.
5. Confirm that the motor-driver enable output is inactive during reset and boot.
6. Connect one motor first and test low PWM values.
7. Measure supply voltage at the driver and at the Wemos while starting and stopping the motor.
8. Test the physical motor-power switch and fuse.
9. Only then place the robot on the floor and test at low speed.

Stop testing immediately if the Wemos resets, the buck converter becomes hot, wiring heats up, the driver smells, or a motor remains active after a stop command.

## Source images

The original JPEGs remain unchanged in this directory. The most relevant technical references are:

- `HTB1QsoCJXXXXXXEXpXXq6xXFXXXp.jpg` — motor test/specification table.
- `HTB1v.mMOFXXXXbWXVXXq6xXFXXX3.jpg` — 9 V / 150 rpm motor parameter summary.
- `HTB1hsknJXXXXXa8XVXXq6xXFXXXF.jpg` — motor product photograph and dimension drawing.
- `HTB1AVxxJpXXXXc8XpXXq6xXFXXXj.jpg` — board/pin-label photograph.
- `HTB1IOFrJpXXXXa1XFXXq6xXFXXXA.jpg` and `HTB1rh8mJpXXXXbhXVXXq6xXFXXXv.jpg` — T100 chassis views.

Some product images contain marketing photographs or Chinese labels rather than electrical specifications. Where the images disagree, verify the physical hardware and use measured values before finalizing the power design.