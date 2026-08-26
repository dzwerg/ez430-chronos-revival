# Changelog

## RC1 - 2026-08-26

First release candidate of the universal RF-free custom firmware.

- Re-enabled guarded LPM3 power saving with reliable button and 1-Hz wake-up.
- Added defensive RF power-down and removed RF/BlueRobin from the linked image.
- Removed acceleration, Agility and Bubble Game from the menu and build.
- Added automatic BMP085/SCP1000 pressure-sensor detection and low-power shutdown.
- Added moon phase, days to next new moon and days to next full moon.
- Expanded TZ2 with Beat Time, 12-hour main-time view, independent DST and SWAP.
- Added seconds-per-week clock fine adjustment, hourly beep and configurable key tones.
- Added configurable menu visibility for BIN, DICE, BATT, MOON, BEAT, ALTI, TEMP and TZ2.
- Improved stopwatch and countdown controls: UP resets, DOWN starts/stops.
- Standardized silent behavior for locked keys; LIGHT is always silent.
- Updated the complete German / English / Tagalog manual for RC1.
- Universal release image is suitable for 433/434, 868 and 915 MHz watch variants
  because no radio configuration is linked.

Build result: 31,068 bytes text, 316 bytes data, 298 bytes BSS.

## v2.3-gcc-beta1

First cleaned public beta candidate.

- Ported the watch firmware to MSP430-GCC / CC430F6137.
- Preserved the stable watchdog-enabled, no-LPM3 input architecture.
- Added configurable hourly beep.
- Added user UTC relationship with DST-aware Beat Time.
- Added TZ2 with half-hour offsets and HH:MM:SS display.
- Expanded Swatch Beat Time to xxx.xx.
- Added binary watch face.
- Added configurable backlight duration and continuous-light mode.
- Added Random MIN/MAX handling.
- Restored real battery ADC sampling and global low-battery indication.
- Added German / English / Tagalog manual and current menu flowcharts.
- Removed IAR project/build artifacts while retaining all `.c` and `.h` source files.


### Development after beta1
- Removed the hidden compile-time dependency on BlueRobin `bm.h` by adding a neutral `include/types.h`.
- Retained TZ2 HH:MM and corrected backlight 20..30 second display layout.


### TZ2 travel workflow (test)
- Added independent TZ2 DLR/DST setting.
- Added Main/TZ2 SWAP with UTC-based time conversion and date rollover handling.


### fixed89
- Standardized DST naming and menu presentation.
- Main/TZ2 SWAP now explicitly preserves and exchanges each zone's DST ON/OFF state.


### fixed90
- Corrected EU DST transition evaluation for a CET/CEST zone while it is in TZ2.
- DST state continues to swap with the corresponding timezone offset.


### fixed91 - guarded power-save test
- Re-enabled LPM3 with strict foreground-idle gating.
- Button debounce/long-press state never sleeps, preventing the old delayed-key behavior.
- Button and 1-Hz timer interrupts wake the foreground.
