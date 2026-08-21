# fixed44 test build

Changes relative to fixed43:
- Countdown default is initialized to 00:05:00 before reset, fixing garbage 88:68:88.
- Countdown and random 1-Hz ticks restored in foreground (not ISR).
- Countdown DOWN start/stop now has a running 1-Hz timebase.
- Agility removed from Line2 menu; sequence is Date -> Stopwatch -> Countdown -> Random -> Battery.
- Random menu uses a steady AVERAGE symbol for identification.
- Backlight auto-off restored (~4 s) in foreground 1-Hz service.
- Accelerometer read/display is performed directly at 20 Hz for X/Y/Z/water-bubble/game views.
- Watchdog and LPM remain disabled for this test build.

## fixed46 BMA250-only correction
- Explicit CC430 port mapping for UCA0 on P1.5/P1.6/P1.7.
- Coherent BMA250 burst read of registers 0x02 through 0x07.
- Correct signed 10-bit axis decoding (LSBs are register bits 7:6).
- No unrelated firmware behavior changed.


## fixed47
- Added Swatch Internet Time / Beat Time immediately after Date in Line2.
- Heart icon is the steady Beat Time indicator.
- Beat calculation uses Biel Mean Time (UTC+1) and subtracts one hour from local CEST when EU DST auto mode is active.
- Countdown is hard-guarded to one decrement per sTime.system_time second.
- Removed legacy cdtimer/random/agility service calls from unused clock_tick() to prevent accidental double ticking.


## fixed48
- Countdown rewritten around a single numeric remaining-seconds counter.
- Exactly one decrement per sTime.system_time value.
- HH:MM:SS regenerated from remaining seconds each tick.
- Full countdown line redraw each tick, eliminating apparent skipped values.
- Beat Time/DST and BMA250 unchanged from fixed47.

## fixed50 alarm-only test build
- UP alone toggles alarm ON/OFF in the Line1 Alarm menu.
- DOWN is untouched and remains a Line2 control.
- Removed dependency on legacy alarm ON/OFF message queue for state changes.
- Time screen keeps alarm icon steady only while enabled.
- Countdown numeric implementation from fixed48 is unchanged.

## fixed53 - alarm UP direct latch
- Alarm-menu UP no longer depends on the generic foreground `pressed` edge.
- P2.4 is sampled directly and latched so one physical press toggles alarm exactly once.
- Latch clears after UP is physically released.
- Other UP functions still use the existing debounced event path.
- Alarm blink, alarm sound, countdown, Beat Time and sensor code unchanged.


## fixed54
- Fixed malformed `BUTTON_UP_IS_RELEASED` macro in `driver/ports.h` (extra closing parenthesis).
- Alarm UP direct-latch logic from fixed53 is unchanged.

fixed58: configurable primary UTC standard offset and TZ2 in 0.5-hour steps; Beat Time derives BMT from UTC and DLR/DST; old hidden Date-menu second timezone removed.

## fixed60 timezone/display correction
- Primary UTC offset remains the standard timezone (e.g. MEZ = UTC+1).
- DLR/DST enable state is explicitly synchronized into the timezone module when clock setup is saved.
- UTC calculation subtracts both the standard UTC offset and the active EU DST hour.
- Swatch Beat Time now displays hundredths (`xxx.xx`).
- TZ2 now displays `HH:MM:SS`.
- UTC/TZ2 offsets remain user-facing real-hour values in 0.5-hour steps (e.g. +5.5 India).


## fixed61 Beat Time overflow/display fix
- Fixed 32-bit overflow in hundredths-of-a-beat calculation.
- Uses mathematically equivalent `(seconds * 125) / 108` instead of `(seconds * 100000) / 86400`.
- Uses the dedicated Line2 decimal-point segment for `xxx.xx`.


## fixed63 beta corrections
- Fixed shared P2.3 LIGHT/backlight handling: when the LED is on the pin now remains output-high continuously and is only sampled as a key input every 10 ms. This prevents the backlight from appearing on only while the key is held while retaining LIGHT long-press access.
- Line2 order changed to Date -> TZ2 -> Swatch Beat Time -> Stopwatch -> Countdown -> Random -> Battery.
- No other application logic changed from fixed62.


## fixed65 beta: watchdog + guarded power management
- Re-enabled the ACLK watchdog (~16 s) only after the stable Timer0/ACLK path is running.
- Foreground services the watchdog on every pass and immediately before LPM3.
- Re-enabled PORT2 wake interrupts for all five buttons.
- Added conservative LPM3 entry only while truly idle.
- LPM3 is suppressed during button debounce/long-press tracking, backlight activity,
  buzzer activity, pending display/sensor work, and active accelerometer measurement.
- Timer0_A0 wakes the CPU once per second, preserving clock/countdown/alarm/hourly logic.
- Radio support remains removed.


## fixed67 beta: LPM button wake restoration
- Fixed a power-management/input regression in fixed65.
- After STAR/NUM long-press set menus, legacy code restored only BUTTON_UP_PIN.
  Consequently STAR, NUM, DOWN and LIGHT no longer woke the CPU immediately
  from LPM3 and their actions/key-beeps waited for a later timer wake.
- All five buttons are now restored as low-to-high PORT2 wake sources after
  every set menu and immediately before idle LPM3 entry.
- Watchdog and the guarded LPM3 power-save remain enabled.
- No functional UI/sensor/timezone/alarm changes were made.


## fixed68 beta: do not sleep while a key is held
- Built on fixed67.
- Found a second LPM3 input regression affecting especially UP/DOWN:
  those actions execute on the debounced press edge, after which the old
  idle test could immediately re-enter LPM3 while the physical key was
  still held.
- Since button IRQs are configured for the press (rising) edge, the release
  edge did not wake the CPU.  Release/debounce was then only observed at a
  later 1-Hz timer wake, making following key actions and their beeps feel delayed.
- LPM3 is now allowed only when stable_buttons == 0 (all buttons released).
- Alarm UP release-latch also blocks LPM3 until its release has been debounced.
- Includes the fixed67 restoration of ALL_BUTTONS as wake sources after set mode.
- No other UI, timing, sensor, timezone or watchface behavior changed.


## fixed69 beta: hard UP debounce in set menus
- Built on fixed68.
- Root cause of huge UP jumps (for example 5 -> 18) was contact bounce generating
  multiple PORT2 rising-edge interrupts while blocking set_value() was active.
- UP now uses a press/release latch inside the PORT2 ISR during set mode:
  first rising edge generates exactly one button.flag.up event, then P2.4 switches
  to the falling (release) edge.  Only after a real release is the next press armed.
- No UP auto-repeat is generated by contact bounce.
- Normal foreground UP handling, alarm logic and LPM3 wake behavior are unchanged.


## fixed70 beta: consolidated set-menu + LPM input fix
- Found the root cause of both set-value jumps and input sluggishness:
  set_value() calls idle_loop() continuously, and idle_loop() always entered LPM3.
  The editor was therefore effectively scheduled by the 200-ms repeat IRQ and
  contact IRQs instead of running continuously.
- While g_set_mode_active is true, idle_loop() no longer enters LPM3.
- The existing long-hold auto-repeat is preserved: button_repeat_on(200) still
  begins repeating after its original delay.
- Both UP and DOWN now use symmetric press/release edge latches in set mode:
  one physical press creates one initial event; bounce cannot create extra events.
- Restored the fixed68 idle condition: normal LPM3 entry is allowed only when
  stable_buttons == 0 (all keys are physically released) and the alarm UP
  release latch is clear.
- Watchdog and normal guarded LPM3 power management remain enabled.


## fixed71 diagnostic: single-source set input + no LPM3
- Both issues persisted through fixed70, so this build removes ambiguity.
- set_value() no longer starts Timer0_A3 button_repeat_on().
- While set_value() is active, STAR/NUM/UP/DOWN PORT2 IRQs are masked and the
  keys are read directly in foreground with 25 ms debounce.
- UP/DOWN: one step per press; after 800 ms continuous hold, auto-repeat starts
  at 200 ms intervals.  This preserves the desired long-hold +++++ behavior
  without a second event source.
- LPM3 is disabled completely in this diagnostic build. Watchdog remains active.
- If normal keys and beeps remain immediate indefinitely, the latency is proven
  to be in the sleep/wakeup design, not the foreground debounce.


## fixed73 stable beta: rollback from fixed72 sleep experiment
- fixed72 is rejected: Alarm UP was broken by the wake-only IRQ change and the
  long-run UI/input state was not stable.
- fixed73 returns exactly to the proven fixed71 runtime architecture:
  * watchdog enabled
  * LPM3 disabled
  * direct debounced set-menu polling
  * controlled long-hold UP/DOWN repeat
  * original working Alarm UP IRQ/toggle path retained
- No menu, sensor, timezone, Beat, BIN, light, countdown or buzzer behavior changed.
- Treat this as the stable enclosure beta baseline.  Power-save should be redesigned
  separately and merged only after it passes long-run input/state tests.


## fixed74 stable beta: battery ADC measurement restored
- Fixed Battery menu being stuck at the hard-coded 3.00 V boot placeholder.
- Entering Battery now immediately requests a real ADC12 AVCC/2 measurement.
- While Battery remains selected, a fresh measurement is requested every 60 seconds.
- The first real sample replaces 3.00 V directly (no smoothing against the placeholder).
- Later samples use a light 30/70 filter.
- Watchdog remains enabled; LPM3 remains disabled exactly as in fixed73.
- No other UI, sensor, alarm, timezone, Beat, BIN, light or countdown behavior changed.


## fixed75 stable beta: global low-battery warning
- Built on fixed74 stable beta.
- Keeps the restored real ADC battery measurement.
- Battery-low threshold is 2.40 V.
- Below 2.40 V, sys.flag.low_battery is set and the battery symbol blinks at 1 Hz
  in every menu, similar to the older TI firmware behavior.
- At or above 2.40 V, the global warning clears.
- No changes to watchdog, sleep policy, input, alarm, sensors, timezone, Beat,
  BIN watchface, light, countdown or random functions.


## fixed78 stable beta: remove legacy battery offset
- Root cause found in the old calibration path: battery_measurement() still added
  sBatt.offset after the new ADC conversion.
- If INFO-D is erased, read_calibration_values() assigns sBatt.offset = -10,
  i.e. subtracts 0.10 V. Existing INFO-D may contain another legacy offset.
- fixed78 deliberately ignores sBatt.offset for battery voltage and converts
  ADC11 (AVCC/2, 2.0-V reference) directly with 32-bit centivolt arithmetic.
- The failed fixed76 TLV correction and fixed77 empirical scaling are not used.
- Real measurement on Battery-menu entry, 60-s refresh and <2.40-V global
  blinking low-battery warning remain unchanged.
- No other firmware behavior changed.


## fixed80 stable beta: ADC reference-state fix
- fixed79 is rejected: ADC12BATMAP is not part of the CC430F6137 ADC12_A
  register set. On this MCU ADC12INCH_11 is already the built-in AVCC/2 battery monitor.
- Found and fixed a real synchronous-ADC bug: REFVSEL was selected with |= and
  only REFVSEL_2 was cleared on shutdown. This could leave stale reference-select
  bits from an earlier temperature/battery conversion.
- The complete REFVSEL field (REFVSEL_3 mask) is now cleared before every
  conversion and again at shutdown, then the requested 1.5/2.0/2.5-V reference
  is selected explicitly.
- Battery conversion remains direct ADC11 AVCC/2 -> centivolts, with no legacy
  battery offset, no TLV factor and no empirical multiplier.
- All fixed78 stable-beta behavior is otherwise unchanged.
