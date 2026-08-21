# eZ430-Chronos Custom Firmware - MSP430-GCC

Target: TI eZ430-Chronos / CC430F6137
Toolchain: MSP430-GCC 9.3.1.11 compatible
Build variants: 433 / 868 / 915 MHz (beta target: 868 MHz)

Build from gcc:
make clean
make FREQUENCY=868 SUPPORT\_DIR=C:/ti/msp430-gcc/include

Output:
gcc/build/868/ez430\_chronos.hex

Flash example:
C:\\ti\\MSPFlasher\_1.3.20\\MSP430Flasher.exe -n CC430F6137 -w build\\868\\ez430\_chronos.hex -v -z \[VCC]

## 

## Active menu

LINE 1 (\*): Time -> Alarm -> Temperature -> Altitude -> Acceleration
LINE 2 (#): Date -> TZ2 -> Beat Time -> Binary Watch -> Stopwatch -> Countdown -> Random -> Battery

## 

## Custom features

* Alarm ON/OFF with UP; long press opens alarm setting.
* Time setup includes hourly double-beep ON/OFF and user UTC offset.
* UTC and DST are used for Beat Time; displayed local time remains user-controlled.
* TZ2 supports half-hour offsets and shows HH:MM:SS.
* Beat Time uses xxx.xx format.
* Binary watch face uses both LCD lines.
* Long LIGHT: backlight setup. DOWN=1..30 seconds, UP=continuous ON/OFF, LIGHT=exit.
* Random setup includes MIN and MAX.
* Battery measurement runs on Battery-menu entry and refreshes while visible.
* Below 2.40 V the battery icon blinks globally.
* Watchdog enabled; LPM3 sleep intentionally disabled in this beta.

## 

## Cleanup policy

All .c and .h source files are retained, including unused modules.
Removed: IAR project/settings files, .r43 objects/libraries, old prebuilt TI-TXT images,
obsolete old manual, and historical runtime-fix note files.

See MANUAL\_DE\_EN\_TL.pdf / .docx.



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

## 

## Wireless / BlueRobin note

BlueRobin support is intentionally not included in this public GCC repository. The old BM innovations header and legacy precompiled BlueRobin/IAR libraries were removed.

