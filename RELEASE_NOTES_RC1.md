# eZ430-Chronos Custom Firmware RC1

RC1 is the first release candidate of the universal RF-free firmware for the
TI eZ430-Chronos / CC430F6137.

## Download

Use `ez430-chronos-custom-rc1-universal.hex` for 433/434, 868 and 915 MHz watch
variants. The image contains no radio stack and powers the RF block down during boot.

## Highlights

- Stable guarded LPM3 power saving and watchdog operation
- Universal RF-free image
- Automatic BMP085/SCP1000 pressure-sensor detection
- Time, alarm, temperature, altitude/pressure and battery functions
- TZ2 with SWAP, DST, Beat Time and 12-hour main-time view
- Binary watch, moon phase, stopwatch, countdown and dice/random generator
- Optional menu visibility settings
- Complete manual in German, English and Tagalog

## Flashing

Example with TI MSP430Flasher:

```powershell
C:\ti\MSPFlasher_1.3.20\MSP430Flasher.exe -n CC430F6137 -w ez430-chronos-custom-rc1-universal.hex -v -z [VCC]
```

Remove and reinsert the watch battery after flashing if a full cold start is desired.

## Build information

- MCU: CC430F6137
- Toolchain: MSP430-GCC 9.3.1.11 compatible
- Flash: 31,068 bytes text + 316 bytes data
- RAM: 316 bytes data + 298 bytes BSS
- SHA-256: included with the GitHub release asset
