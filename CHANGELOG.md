# Changelog

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
