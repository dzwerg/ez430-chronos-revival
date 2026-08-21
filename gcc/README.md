# eZ430-Chronos V2.3 - MSP430-GCC 9.3.1.11 port

Target: TI CC430F6137.
Toolchain target: MSP430-GCC 9.3.1.11 (GCC 9.3.1.11 / binutils 2.34 / Newlib 2.4.0).

Build from this directory with `make FREQUENCY=915`. Valid frequencies: 433, 868, 915.

BlueRobin support and the legacy proprietary `.r43` receiver are not included in this cleaned public GCC tree.

### GCC 9.3.1.11 note: THIS_DEVICE_ADDRESS
The SimpliciTI device address must be passed as a C brace initializer, not as a string literal.
The Makefile therefore uses:
`-DTHIS_DEVICE_ADDRESS={0x79,0x56,0x34,0x12}`
