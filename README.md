# eZ430-Chronos Custom Firmware RC1

Target: TI eZ430-Chronos / CC430F6137
Toolchain: MSP430-GCC 9.3.1.11 compatible
Release: RC1
Build variants: 433 / 868 / 915 MHz

RC1 uses one universal RF-free firmware image for all supported watch-frequency
variants. The frequency parameter only keeps the historical build-directory
layout; it does not enable the radio.

Build from gcc:
    make clean
    make FREQUENCY=868 SUPPORT_DIR=C:/ti/msp430-gcc/include

Build output:
    gcc/build/868/ez430_chronos.hex

Flash example:
    C:\ti\MSPFlasher_1.3.20\MSP430Flasher.exe -n CC430F6137 -w build\868\ez430_chronos.hex -v -z [VCC]

RC1 menu
-----------
LINE 1 (*): Time -> Alarm -> Temperature -> Altitude/Pressure -> Battery
LINE 2 (#): Date -> TZ2 -> Beat Time -> Binary Watch -> Moon -> Stopwatch -> Countdown -> Dice

Custom features
---------------
- Guarded LPM3 power saving with reliable button wake-up and watchdog protection.
- Alarm ON/OFF with UP; long press opens alarm setting.
- Time setup: 12/24 h, time, seconds-per-week fine adjustment, DST, hourly beep,
  key tones and UTC offset.
- TZ2 with half-hour offsets, independent DST, Main/TZ2 SWAP, Beat view and a
  12-hour main-time view with seconds and PM indicator.
- Full-screen Swatch Internet Time in xxx.xx format.
- Binary watch face and moon phase / days-to-NEW / days-to-FULL views.
- Stopwatch: DOWN start/stop, UP reset, STOP/LAP mode retained.
- Countdown: DOWN start/stop, UP restore configured start value.
- Long LIGHT: backlight setup. DOWN=1..30 seconds, UP=continuous ON/OFF, LIGHT=exit.
- Dice/random setup includes MIN, MAX, MODE and STYL.
- Battery view shows BATT above and measured voltage below.
- Below 2.40 V the battery icon blinks globally.
- Optional menu items can be hidden from a nested DATE setup menu: BIN, DICE,
  BATT, MOON, BEAT, ALTI, TEMP and TZ2.
- Automatic pressure-sensor detection for BMP085 (white PCB) and SCP1000
  (older black PCB); the sensor is shut down or put in standby on exit.
- Radio/BlueRobin, acceleration, Agility and Bubble Game are intentionally removed.

Cleanup policy
--------------
Legacy source files may remain for reference, but RF and motion modules are excluded
from the RC1 build.
Removed: IAR project/settings files, .r43 objects/libraries, old prebuilt TI-TXT images,
obsolete old manual, and historical runtime-fix note files.

See `MANUAL_DE_EN_TL.pdf` or the editable `MANUAL_DE_EN_TL.docx` for the complete
German, English and Tagalog manual.


## Project origin, licensing and credits

This project continues the Texas Instruments eZ430-Chronos firmware and keeps
the original copyright/license headers in the source files. The cleaned public
repository is distributed under the BSD-3-Clause license.

The current GCC modernization, feature work, debugging and documentation were
developed interactively by the project maintainer with assistance from
**ChatGPT by OpenAI**. ChatGPT was used as a development assistant for code
review, porting, debugging and documentation. OpenAI is not the maintainer or
sponsor of this repository, and this acknowledgement does not imply endorsement.

The goal of this repository is preservation: keeping the eZ430-Chronos useful,
buildable and modifiable with a modern MSP430-GCC toolchain while continuing to
share the source code openly under the applicable upstream terms.

## Wireless / BlueRobin note

BlueRobin support is intentionally not included in this public GCC repository. The old BM innovations header and legacy precompiled BlueRobin/IAR libraries were removed.

## DST rule

DST automation currently follows the EU/CET-CEST rule. This means a UTC+1 zone with DST enabled continues to switch automatically between CET and CEST even when it is stored in TZ2 after a timezone SWAP.
