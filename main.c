// *************************************************************************************************
//
//	Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/ 
//	 
//	 
//	  Redistribution and use in source and binary forms, with or without 
//	  modification, are permitted provided that the following conditions 
//	  are met:
//	
//	    Redistributions of source code must retain the above copyright 
//	    notice, this list of conditions and the following disclaimer.
//	 
//	    Redistributions in binary form must reproduce the above copyright
//	    notice, this list of conditions and the following disclaimer in the 
//	    documentation and/or other materials provided with the   
//	    distribution.
//	 
//	    Neither the name of Texas Instruments Incorporated nor the names of
//	    its contributors may be used to endorse or promote products derived
//	    from this software without specific prior written permission.
//	
//	  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//	  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//	  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//	  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//	  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//	  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//	  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//	  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//	  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//	  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//	  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *************************************************************************************************
// Initialization and control of application.
// *************************************************************************************************

// *************************************************************************************************
// Include section

// system
#include "project.h"
#include <string.h>

// driver
#include "clock.h"
#include "display.h"
#include "vti_as.h"
#include "vti_ps.h"
#include "buzzer.h"
#include "ports.h"
#include "timer.h"
#include "pmm.h"

// logic
#include "menu.h"
#include "date.h"
#include "alarm.h"
#include "stopwatch.h"
#include "battery.h"
#include "temperature.h"
#include "altitude.h"
#include "battery.h"
#include "acceleration.h"
#include "countdowntimer.h"
#include "random.h"
#include "agility.h"


// *************************************************************************************************
// Prototypes section
void init_application(void);
void init_global_variables(void);
void wakeup_event(void);
void process_requests(void);
void display_update(void);
void idle_loop(void);
void configure_ports(void);
void read_calibration_values(void);
static void backlight_setup(void);


// *************************************************************************************************
// Defines section

// Number of calibration data bytes in INFOA memory
#define CALIBRATION_DATA_LENGTH		(13u)


// *************************************************************************************************
// Global Variable section

// Variable holding system internal flags
volatile s_system_flags sys;

// Variable holding flags set by logic modules 
volatile s_request_flags request;

// Variable holding message flags
volatile s_message_flags message;

// For Backlight: BL remains on for about 4sec also after button pressed
u8 BlOnFlag = 0;
/* fixed62: user-configurable backlight behaviour. */
u8 BacklightDurationSeconds = 4;
u8 BacklightAlwaysOn = 0;

// fixed27: set by Timer0_A0 ISR, consumed in the foreground loop.
volatile u8 fixed27_tick_pending = 0;
/* fixed37: PORT2 ISR only feeds button flags while a blocking set menu is active. */
volatile u8 g_set_mode_active = 0;
volatile u8 g_alarm_up_irq = 0; /* fixed55: reliable UP event from PORT2 ISR */

// Function pointers for LINE1 and LINE2 display function 
void (*fptr_lcd_function_line1)(u8 line, u8 update);
void (*fptr_lcd_function_line2)(u8 line, u8 update);

// *************************************************************************************************
// Extern section

extern u16 ps_read_register(u8 address, u8 mode);
extern u8 ps_write_register(u8 address, u8 data);

// *************************************************************************************************
// fixed21 boot checkpoint diagnostics
// Patterns are written directly to LCD memory so they do not depend on the menu/display code.
static void bootdiag_lcd_init(void)
{
    volatile u8 *lcd = (volatile u8 *)0x0A20;
    u8 i;
    WDTCTL = WDTPW | WDTHOLD;
#ifdef SELA_7
    UCSCTL4 = (UCSCTL4 & ~SELA_7) | SELA__REFOCLK;
#endif
    P5SEL |= BIT5 | BIT6 | BIT7;
    P5DIR |= BIT5 | BIT6 | BIT7;
    LCDBMEMCTL |= LCDCLRBM | LCDCLRM;
    LCDBCTL0 = LCDDIV0 | LCDDIV1 | LCDDIV2 | LCDDIV3 |
               LCDPRE0 | LCDPRE1 | LCD4MUX | LCDON;
    LCDBPCTL0 = 0xFFFF;
    LCDBPCTL1 = 0x00FF;
    LCDBVCTL = LCDCPEN | VLCD_2_72;
    for (i = 0; i < 12; ++i) lcd[i] = 0;
}

static void bootdiag_pattern(u8 value)
{
    volatile u8 *lcd = (volatile u8 *)0x0A20;
    u8 i;
    for (i = 0; i < 12; ++i) lcd[i] = value;
}

static void bootdiag_hold(void)
{
    volatile unsigned long n;
    for (n = 0; n < 500000UL; ++n) __no_operation();
}

/* fixed51: restore the Chronos buzzer hardware mapping which was lost when
   the early generic port-mapping block was removed. P2.7 carries TA1CCR0A. */
static void buzzer_portmap_init(void)
{
    PMAPPWD = 0x02D52u;
#ifdef PMAPRECFG
    PMAPCTL |= PMAPRECFG;
#endif
    {
        volatile unsigned char *pmap = &P2MAP0;
        *(pmap + 7) = PM_TA1CCR0A;
    }
    PMAPPWD = 0;

    P2SEL &= ~BIT7;
    P2OUT &= ~BIT7;
    P2DIR |= BIT7;
}

/* Short, non-blocking key click. The actual 2.7-kHz tone is generated by
   TA1; Timer0_A3 only controls the 20-ms envelope. */
static void key_beep(void)
{
    if (!is_buzzer())
        start_buzzer(1, BUZZER_ON_TICKS, BUZZER_OFF_TICKS);
}

/* fixed62: light-button setup. DOWN cycles 1..30 seconds, UP toggles
   continuous mode, and a short LIGHT press stores/exits.  It intentionally
   polls the pins directly so it does not depend on the blocking set_value()
   button-repeat path. */
static void backlight_setup_draw(void)
{
    u8 t[7];
    u8 sec = BacklightDurationSeconds;
    clear_display();
    display_chars(LCD_SEG_L1_3_0, (u8 *)"LITE", SEG_ON);
    t[0] = (sec >= 10) ? (u8)('0' + (sec / 10)) : (u8)' ';
    t[1] = (u8)('0' + (sec % 10));
    t[2] = (u8)' ';
    if (BacklightAlwaysOn)
    {
        t[3] = (u8)' ';
        t[4] = (u8)'O';
        t[5] = (u8)'N';
    }
    else
    {
        t[3] = (u8)'O';
        t[4] = (u8)'F';
        t[5] = (u8)'F';
    }
    t[6] = 0;
    display_chars(LCD_SEG_L2_5_0, t, SEG_ON);
}

static void backlight_setup(void)
{
    u8 old = BUTTONS_IN & ALL_BUTTONS;
    u8 armed = 0;
    u16 changed = TA0R;
    const u16 debounce = (u16)((32768UL * 20UL) / 1000UL);

    /* Make sure the shared light/button pin is an input while editing. */
    BUTTONS_DIR &= ~BUTTON_BACKLIGHT_PIN;
    BUTTONS_OUT &= ~BUTTON_BACKLIGHT_PIN;
    BUTTONS_DS  &= ~BUTTON_BACKLIGHT_PIN;
    backlight_setup_draw();

    while (1)
    {
        u8 now = BUTTONS_IN & ALL_BUTTONS;
        if (now != old)
        {
            old = now;
            changed = TA0R;
            armed = 0;
        }
        else if (!armed && ((u16)(TA0R - changed) >= debounce))
        {
            armed = 1;
            if (now & BUTTON_DOWN_PIN)
            {
                BacklightDurationSeconds++;
                if (BacklightDurationSeconds > 30) BacklightDurationSeconds = 1;
                key_beep();
                backlight_setup_draw();
            }
            else if (now & BUTTON_UP_PIN)
            {
                BacklightAlwaysOn ^= 1;
                key_beep();
                backlight_setup_draw();
            }
            else if (now & BUTTON_BACKLIGHT_PIN)
            {
                key_beep();
                /* Wait for release so the exit key does not leak into main. */
                while (BUTTONS_IN & BUTTON_BACKLIGHT_PIN) { }
                break;
            }
        }
        if (now == 0) armed = 0;
    }

    clear_display();
    display.flag.full_update = 1;
    display_update();
}

// *************************************************************************************************
// @fn          main
// @brief       Main routine
// @param       none
// @return      none
// *************************************************************************************************
int main(void)
{
    /* Foreground button debounce state. TA0R runs from ACLK at 32768 Hz. */
    u8 raw_buttons = 0;
    u8 stable_buttons = 0;
    u16 buttons_changed_at = 0;
    const u16 button_debounce_ticks = (u16)((32768UL * 20UL) / 1000UL);
    const u16 button_long_ticks = (u16)32768UL; /* 1.0 s at 32768 Hz */
    u16 star_pressed_at = 0;
    u16 num_pressed_at = 0;
    u16 backlight_pressed_at = 0;
    u8 star_tracking = 0;
    u8 num_tracking = 0;
    u8 backlight_tracking = 0;
    u8 star_long = 0;
    u8 num_long = 0;
    u8 backlight_long = 0;
    /* fixed63: P2.3 is shared between the LIGHT key and the LED drive.
       Do not switch it back to input on every foreground loop: that made
       the LED spend most of its time undriven and appear to work only while
       the physical key was held. Sample the key only every 10 ms and keep
       the pin as LED output between samples. */
    u16 backlight_sample_at = 0;
    u8 backlight_sample_state = 0;
    const u16 backlight_sample_ticks = (u16)((32768UL * 10UL) / 1000UL);
    u16 accel_poll_at = 0;
    const u16 accel_poll_ticks = (u16)((32768UL * 50UL) / 1000UL); /* 20 Hz */
    u8 alarm_blink_phase = 1;
    u8 alarm_up_lock = 0;
    u8 alarm_up_release_tracking = 0;
    u16 alarm_up_release_at = 0;
    u8 battery_refresh_seconds = 0;
    u8 battery_menu_active_prev = 0;
    u8 low_batt_blink_phase = 0;

    /* Start from the proven-good LCD/ACLK bootstrap used by fixed28. */
    bootdiag_lcd_init();

    /* fixed65 beta: radio stays disabled; watchdog and guarded LPM3 are enabled. */
    button.all_flags  = 0;
    sys.all_flags     = 0;
    request.all_flags = 0;
    display.all_flags = 0;
    message.all_flags = 0;
    sys.flag.use_metric_units = 1;

    /* Safe menu defaults.  Function pointers MUST be valid before any
       display helper such as display_update()/clear_display_all() can run. */
    ptrMenu_L1 = &menu_L1_Time;
    ptrMenu_L2 = &menu_L2_Date;
    fptr_lcd_function_line1 = ptrMenu_L1->display_function;
    fptr_lcd_function_line2 = ptrMenu_L2->display_function;

    /* Initialize only the modules exposed by the fixed29 menu. */
    read_calibration_values();
    reset_clock();
    sTime.drawFlag = 3;
    reset_date();
    reset_alarm();
    reset_stopwatch();
    reset_buzzer();
    buzzer_portmap_init();
    reset_batt_measurement();
    /* fixed44: initialize countdown default before reset_cdtimer().
       Previously defaultTime was still zero/uninitialized here, producing
       garbage such as 88:68:88 in the set screen. */
    memcpy(scdtimer.defaultTime, "000500", sizeof(scdtimer.time));
    reset_cdtimer();
    reset_random();
    reset_acceleration();
    reset_agility();

    /* Prepare the accelerometer interface, but do not start sampling until
       the Agility menu is explicitly started by the user. */
    as_init();

    /* Poll the five buttons in the foreground.  Port interrupts for the
       buttons remain disabled in this stable build.  The accelerometer may
       still enable its own P2 interrupt when Agility is started. */
    BUTTONS_DIR &= ~ALL_BUTTONS;
    BUTTONS_OUT &= ~ALL_BUTTONS;
    BUTTONS_REN |= ALL_BUTTONS;
    BUTTONS_IE  &= ~ALL_BUTTONS;
    BUTTONS_IFG &= ~ALL_BUTTONS;

    /* First real menu display. */
    clear_display();
    display.flag.full_update = 1;
    display_update();

    /* Start the proven 1 Hz timer before any service that depends on
       Timer0_A4/ADC interrupts.  fixed30 called battery_measurement() here
       too early and deadlocked before the main loop. */
    Timer0_Init();
    TA0CCTL0 &= ~CCIFG;
    fixed27_tick_pending = 0;
    __enable_interrupt();

    /* fixed65 beta: watchdog runs from ACLK with the original ~16 s period.
       Start it only after the stable clock/timer path is alive. */
#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif

    /* fixed65 beta: all buttons are wake sources while the CPU sleeps.
       The foreground state machine still performs debounce/short/long press. */
    BUTTONS_IFG &= (u8)~ALL_BUTTONS;
    BUTTONS_IES &= (u8)~ALL_BUTTONS; /* low->high = press */
    BUTTONS_IE  |= ALL_BUTTONS;

    /* fixed55: keep UP button IRQ enabled outside set mode.
       This is the same hardware path that already works reliably in set_value(). */
    BUTTONS_IFG &= (u8)~BUTTON_UP_PIN;
    BUTTONS_IES &= (u8)~BUTTON_UP_PIN; /* low->high = press on Chronos buttons */
    BUTTONS_IE  |= ALL_BUTTONS;

    /* fixed41: core-first startup.  Sensors are reset only after Timer0/GIE.
       BMP085 hardware probing is deferred until the Altitude menu is actually opened. */
    reset_temp_measurement();
    reset_altitude_measurement();
    set_blink_rate(BIT4 + BIT3);

    /* Do NOT take an ADC battery sample during boot.  sBatt starts at 3.00 V.
       Battery measurements requested later run with Timer0 and GIE active. */

    /* Seed debounce state after Timer0 has started. */
    raw_buttons = BUTTONS_IN & ALL_BUTTONS;
    stable_buttons = raw_buttons;
    buttons_changed_at = TA0R;
    backlight_sample_at = TA0R;
    backlight_sample_state = raw_buttons & BUTTON_BACKLIGHT_PIN;
    accel_poll_at = TA0R;

    while (1)
    {
        u8 now;
        u8 pressed = 0;

#ifdef USE_WATCHDOG
        /* The 1-Hz Timer0 interrupt guarantees regular wakeups from LPM3. */
        WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif

        /* fixed63: P2.3 is shared by LIGHT and the LED drive.
           While the backlight is ON, keep P2.3 as an output almost all the
           time. Only every 10 ms switch it briefly to input to sample the
           physical key, then immediately restore LED drive. This preserves
           long-press detection without PWM-like dimming of the backlight. */
        if (BlOnFlag)
        {
            u8 other_buttons = BUTTONS_IN & (u8)(ALL_BUTTONS & (u8)~BUTTON_BACKLIGHT_PIN);
            if ((u16)(TA0R - backlight_sample_at) >= backlight_sample_ticks)
            {
                backlight_sample_at = TA0R;
                BUTTONS_DIR &= (u8)~BUTTON_BACKLIGHT_PIN;
                BUTTONS_OUT &= (u8)~BUTTON_BACKLIGHT_PIN;
                __no_operation();
                __no_operation();
                __no_operation();
                backlight_sample_state = BUTTONS_IN & BUTTON_BACKLIGHT_PIN;
                BUTTONS_OUT |= BUTTON_BACKLIGHT_PIN;
                BUTTONS_DIR |= BUTTON_BACKLIGHT_PIN;
                BUTTONS_DS  |= BUTTON_BACKLIGHT_PIN;
            }
            now = other_buttons | backlight_sample_state;
        }
        else
        {
            now = BUTTONS_IN & ALL_BUTTONS;
            backlight_sample_state = now & BUTTON_BACKLIGHT_PIN;
        }
        u8 released = 0;

        /* fixed33: debounce first, then distinguish SHORT and LONG presses.
           STAR/NUM short actions are deliberately executed on RELEASE, not
           on the initial press. This prevents a long press from first moving
           to the next menu item. */
        if (now != raw_buttons)
        {
            raw_buttons = now;
            buttons_changed_at = TA0R;
        }
        else if ((stable_buttons != raw_buttons) &&
                 ((u16)(TA0R - buttons_changed_at) >= button_debounce_ticks))
        {
            u8 old_stable = stable_buttons;
            stable_buttons = raw_buttons;
            pressed  = stable_buttons & (u8)~old_stable;
            released = old_stable & (u8)~stable_buttons;
        }

        /* fixed52: any physical key acknowledges an active alarm before the
           normal key action is processed. Consume this press so a wake-up
           key cannot also change a menu item. */
        if ((sAlarm.state == ALARM_ON) && pressed)
        {
            stop_alarm();
            pressed = 0;
            released = 0;
            star_tracking = 0;
            num_tracking = 0;
            display.flag.line1_full_update = 1;
            display_update();
        }

        if (pressed & BUTTON_STAR_PIN)
        {
            star_tracking = 1;
            star_long = 0;
            star_pressed_at = TA0R;
        }
        if (pressed & BUTTON_NUM_PIN)
        {
            num_tracking = 1;
            num_long = 0;
            num_pressed_at = TA0R;
        }
        if (pressed & BUTTON_BACKLIGHT_PIN)
        {
            backlight_tracking = 1;
            backlight_long = 0;
            backlight_pressed_at = TA0R;
        }

        if (star_tracking && !star_long && (stable_buttons & BUTTON_STAR_PIN) &&
            ((u16)(TA0R - star_pressed_at) >= button_long_ticks))
            star_long = 1;

        if (num_tracking && !num_long && (stable_buttons & BUTTON_NUM_PIN) &&
            ((u16)(TA0R - num_pressed_at) >= button_long_ticks))
            num_long = 1;

        if (backlight_tracking && !backlight_long &&
            (stable_buttons & BUTTON_BACKLIGHT_PIN) &&
            ((u16)(TA0R - backlight_pressed_at) >= button_long_ticks))
            backlight_long = 1;

        /* STAR: short release advances Line1; long release enters mx/set mode. */
        if (released & BUTTON_STAR_PIN)
        {
            key_beep();
            /* fixed64: BIN is a full-screen watch face.  While it is active,
               STAR must not move/change Line1 underneath it. */
            if (ptrMenu_L2 == &menu_L2_BinWatch)
            {
                star_tracking = 0;
                star_long = 0;
                raw_buttons = BUTTONS_IN & ALL_BUTTONS;
                stable_buttons = raw_buttons;
                buttons_changed_at = TA0R;
            }
            else if (star_tracking && star_long && ptrMenu_L1->mx_function)
            {
                button.all_flags = 0;
                BUTTONS_IFG = 0;
                BUTTONS_IE |= ALL_BUTTONS;   /* minimal PORT2 ISR feeds set_value() */
                g_set_mode_active = 1;
                ptrMenu_L1->mx_function(LINE1);
                g_set_mode_active = 0;
                BUTTONS_IE &= ~ALL_BUTTONS;
                BUTTONS_IFG &= (u8)~ALL_BUTTONS;
                BUTTONS_IES &= (u8)~ALL_BUTTONS;
                BUTTONS_IE |= ALL_BUTTONS;
                BUTTONS_IFG = 0;
                button.all_flags = 0;
                display.flag.full_update = 1;
                display_update();
            }
            else if (star_tracking)
            {
                fptr_lcd_function_line1(LINE1, DISPLAY_LINE_CLEAR);
                ptrMenu_L1 = ptrMenu_L1->next;
                fptr_lcd_function_line1 = ptrMenu_L1->display_function;
                display.flag.line1_full_update = 1;
                display_update();
            }
            star_tracking = 0;
            star_long = 0;
            raw_buttons = BUTTONS_IN & ALL_BUTTONS;
            stable_buttons = raw_buttons;
            buttons_changed_at = TA0R;
        }

        /* NUM: short release advances Line2; long release enters mx/set mode. */
        if (released & BUTTON_NUM_PIN)
        {
            key_beep();
            if (num_tracking && num_long && ptrMenu_L2->mx_function)
            {
                button.all_flags = 0;
                BUTTONS_IFG = 0;
                BUTTONS_IE |= ALL_BUTTONS;
                g_set_mode_active = 1;
                ptrMenu_L2->mx_function(LINE2);
                g_set_mode_active = 0;
                BUTTONS_IE &= ~ALL_BUTTONS;
                BUTTONS_IFG &= (u8)~ALL_BUTTONS;
                BUTTONS_IES &= (u8)~ALL_BUTTONS;
                BUTTONS_IE |= ALL_BUTTONS;
                BUTTONS_IFG = 0;
                button.all_flags = 0;
                display.flag.full_update = 1;
                display_update();
            }
            else if (num_tracking)
            {
                fptr_lcd_function_line2(LINE2, DISPLAY_LINE_CLEAR);
                ptrMenu_L2 = ptrMenu_L2->next;
                fptr_lcd_function_line2 = ptrMenu_L2->display_function;
                display.flag.line2_full_update = 1;
                display_update();
            }
            num_tracking = 0;
            num_long = 0;
            raw_buttons = BUTTONS_IN & ALL_BUTTONS;
            stable_buttons = raw_buttons;
            buttons_changed_at = TA0R;
        }

        /* fixed62: LIGHT short/long are distinguished on RELEASE, just like
           STAR/NUM.  This avoids switching the shared P2.3 pin to LED output
           before we know whether the user intends a long press. */
        if (released & BUTTON_BACKLIGHT_PIN)
        {
            extern u8 BlTimeoutCounter;
            key_beep();
            if (backlight_tracking && backlight_long)
            {
                backlight_setup();
            }
            else if (backlight_tracking)
            {
                BlOnFlag = 1;
                BlTimeoutCounter = 0;
                backlight_sample_state = 0; /* key was just released */
                backlight_sample_at = TA0R;
                BUTTONS_OUT |= BUTTON_BACKLIGHT_PIN;
                BUTTONS_DIR |= BUTTON_BACKLIGHT_PIN;
                BUTTONS_DS  |= BUTTON_BACKLIGHT_PIN;
            }
            backlight_tracking = 0;
            backlight_long = 0;
            raw_buttons = (BUTTONS_IN & (u8)(ALL_BUTTONS & (u8)~BUTTON_BACKLIGHT_PIN)) |
                          backlight_sample_state;
            stable_buttons = raw_buttons;
            buttons_changed_at = TA0R;
        }

        /* fixed57: Alarm UP is handled exclusively by the PORT2 IRQ.
           A press/release latch guarantees exactly one ON/OFF toggle per
           physical key press. The normal foreground UP handler deliberately
           ignores the Alarm menu to avoid double-processing the same key. */
        if (g_alarm_up_irq)
        {
            g_alarm_up_irq = 0;
            if ((ptrMenu_L1 == &menu_L1_Alarm) && !alarm_up_lock)
            {
                alarm_up_lock = 1;
                alarm_up_release_tracking = 0;
                key_beep();
                button.flag.up = 1;
                sx_alarm(LINE1);
                button.flag.up = 0;
                display.flag.line1_full_update = 1;
                display_update();
            }
        }

        if (alarm_up_lock)
        {
            if (BUTTON_UP_IS_RELEASED)
            {
                if (!alarm_up_release_tracking)
                {
                    alarm_up_release_tracking = 1;
                    alarm_up_release_at = TA0R;
                }
                else if ((u16)(TA0R - alarm_up_release_at) >= button_debounce_ticks)
                {
                    alarm_up_lock = 0;
                    alarm_up_release_tracking = 0;
                    BUTTONS_IFG &= (u8)~BUTTON_UP_PIN;
                }
            }
            else
            {
                alarm_up_release_tracking = 0;
            }
        }

        /* UP/DOWN/BACKLIGHT remain immediate-on-press after debounce. */
        if (pressed & BUTTON_UP_PIN)
        {
            /* Alarm UP is handled only by the IRQ+latch block above. */
            if (ptrMenu_L1 != &menu_L1_Alarm)
            {
                key_beep();
                button.flag.up = 1;
                if (ptrMenu_L1->sx_function)
                    ptrMenu_L1->sx_function(LINE1);
                button.flag.up = 0;
                display.flag.line1_full_update = 1;
                display_update();
            }
        }
        else if (pressed & BUTTON_DOWN_PIN)
        {
            key_beep();
            button.flag.down = 1;
            if (ptrMenu_L2->sx_function)
                ptrMenu_L2->sx_function(LINE2);
            button.flag.down = 0;
            display.flag.line2_full_update = 1;
            display_update();
        }

        if (fixed27_tick_pending)
        {
            extern u8 BlTimeoutCounter;
            __disable_interrupt();
            fixed27_tick_pending = 0;
            __enable_interrupt();

            /* fixed44: restore only the safe 1-Hz application services which
               were lost when clock_tick() was removed from the ISR. */
            cdtimer_tick();
            random_tick();

            if (ptrMenu_L2 == &menu_L2_Battery)
            {
                if (++battery_refresh_seconds >= 60u)
                {
                    battery_refresh_seconds = 0;
                    request.flag.voltage_measurement = 1;
                }
            }

            /* fixed75: global low-battery warning.
               Below 2.40 V the battery icon blinks in every menu.
               Above the threshold it is off, except Battery menu may draw it itself. */
            if (sys.flag.low_battery)
            {
                low_batt_blink_phase ^= 1u;
                display_symbol(LCD_SYMB_BATTERY,
                               low_batt_blink_phase ? SEG_ON : SEG_OFF);
            }
            else
            {
                low_batt_blink_phase = 0;
                if (ptrMenu_L2 != &menu_L2_Battery)
                    display_symbol(LCD_SYMB_BATTERY, SEG_OFF);
            }

            /* fixed62: configurable 1..30 s timeout. In continuous mode the
               light remains on until configuration is changed/reset. */
            if (BlOnFlag && !BacklightAlwaysOn)
            {
                if (++BlTimeoutCounter >= BacklightDurationSeconds)
                {
                    BlOnFlag = 0;
                    BlTimeoutCounter = 0;
                    BUTTONS_DIR &= ~BUTTON_BACKLIGHT_PIN;
                    BUTTONS_OUT &= ~BUTTON_BACKLIGHT_PIN;
                    BUTTONS_DS  &= ~BUTTON_BACKLIGHT_PIN;
                }
            }

            /* fixed52: evaluate the alarm only at the exact start of a
               minute. This prevents an acknowledged alarm from retriggering
               repeatedly during the same matching minute. */
            if (sTime.second == 0)
                check_alarm();

            /* fixed57: optional full-hour signal. Swatch/clock time itself
               already includes the user's DST setting, so the chime follows
               the displayed local hour. Alarm has priority over the chime. */
            if (HourlyBeepFlag &&
                (sTime.minute == 0) && (sTime.second == 0) &&
                (sAlarm.state != ALARM_ON) && !is_buzzer())
            {
                start_buzzer(2, BUZZER_ON_TICKS, BUZZER_OFF_TICKS);
            }

            /* fixed56: expire the original-style ON/OFF alarm message. */
            alarm_ui_tick();

            /* Repeating alarm pattern: two short beeps once per second for
               ALARM_ON_DURATION seconds, or until any key acknowledges it. */
            if (sAlarm.state == ALARM_ON)
            {
                if (sAlarm.duration > 0)
                {
                    if (!is_buzzer())
                        start_buzzer(2, BUZZER_ON_TICKS, BUZZER_OFF_TICKS);
                    sAlarm.duration--;
                }
                else
                {
                    stop_alarm();
                }
            }

            display_update();

            /* The old LCD_B hardware blink setup proved unreliable in the
               GCC port. Blink the alarm-menu icon explicitly at 1 Hz. */
            if (ptrMenu_L1 == &menu_L1_Alarm)
            {
                alarm_blink_phase ^= 1u;
                display_symbol(LCD_ICON_ALARM,
                               alarm_blink_phase ? SEG_ON_BLINK_OFF : SEG_OFF_BLINK_OFF);
            }
        }

        /* fixed44: read acceleration directly at 20 Hz.  This makes the
           X/Y/Z, water-bubble and bubble-game views react immediately and
           avoids losing/coalescing a foreground request flag. */
        if (is_acceleration_measurement() &&
            ((u16)(TA0R - accel_poll_at) >= accel_poll_ticks))
        {
            accel_poll_at = TA0R;
            do_acceleration_measurement();
            if (ptrMenu_L1 == &menu_L1_Acceleration)
                display_update();
        }

        /* fixed74: Battery menu is no longer a static 3.00 V placeholder.
           Trigger a real ADC sample on entry; then refresh once per minute
           while the menu remains visible. */
        if (ptrMenu_L2 == &menu_L2_Battery)
        {
            if (!battery_menu_active_prev)
            {
                battery_menu_active_prev = 1;
                battery_refresh_seconds = 0;
                request.flag.voltage_measurement = 1;
            }
        }
        else
        {
            battery_menu_active_prev = 0;
            battery_refresh_seconds = 0;
        }

        /* Service timer/sensor requests continuously in foreground context. */
        if (request.all_flags)
        {
            process_requests();

            if (sys.flag.low_battery)
                display_symbol(LCD_SYMB_BATTERY, SEG_ON);
            else if (ptrMenu_L2 != &menu_L2_Battery)
                display_symbol(LCD_SYMB_BATTERY, SEG_OFF);
        }
        if (display.all_flags)
            display_update();
        /* fixed73 stable beta: LPM3 remains deliberately disabled.
           fixed71 proved this input architecture stable. Watchdog remains active. */
    }
}


// *************************************************************************************************
// @fn          init_ucs
// @brief       Initialize the Unified Clock System.
// @param       none
// @return      none
// *************************************************************************************************
void init_ucs(void)
{
	// fixed19 diagnostic: do not depend on the 32-kHz crystal. Use the
	// internal REFO (~32768 Hz) for ACLK and as the FLL reference.
	// This keeps Timer0/RTC-style ticks alive while isolating XT1 faults.
	P5SEL &= ~(BIT0 | BIT1);
	UCSCTL6 |= XT1OFF;
	UCSCTL3 = SELREF__REFOCLK;
	UCSCTL4 = SELA__REFOCLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;

	__bis_SR_register(SCG0);
	UCSCTL0 = 0x0000;
	UCSCTL1 = DCORSEL_5;
	UCSCTL2 = FLLD_1 + 0x16E;
	__bic_SR_register(SCG0);
	__delay_cycles(250000);

	// Clear DCO/oscillator fault flags, but never wait on XT1.
	UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
	SFRIFG1 &= ~OFIFG;
}




// *************************************************************************************************
// @fn          init_application
// @brief       Initialize the microcontroller.
// @param       none
// @return      none
// *************************************************************************************************
void init_application(void)
{
	  
	// ---------------------------------------------------------------------
	// Enable watchdog
	
	// Watchdog triggers after 16 seconds when not cleared
#ifdef USE_WATCHDOG		
	WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK;
#else
	WDTCTL = WDTPW + WDTHOLD;
#endif
	
	// ---------------------------------------------------------------------
	// Configure PMM
	SetVCore(3);
	// fixed21 checkpoint 1: VCore setup returned.
	bootdiag_pattern(0xAA); bootdiag_hold();
	
	// Set global high power request enable
	PMMCTL0_H  = 0xA5;
	PMMCTL0_L |= PMMHPMRE;
	PMMCTL0_H  = 0x00;	

	// ---------------------------------------------------------------------
    init_ucs();
	// fixed21 checkpoint 2: UCS setup returned.
	bootdiag_pattern(0x55); bootdiag_hold();
	// ---------------------------------------------------------------------
	// Configure port mapping
	
	// fixed22: keep GIE disabled through the whole early init and SKIP
	// port mapping completely. The basic LCD/clock UI does not need it.
	// This isolates a reset observed immediately after UCS setup.
	__disable_interrupt();
	bootdiag_pattern(0x0F); bootdiag_hold();
	
	// ---------------------------------------------------------------------
	// Configure ports

	// ---------------------------------------------------------------------
	// fixed19 diagnostic: skip external acceleration sensor initialization.
	// as_init();
	
	// ---------------------------------------------------------------------
	// Init LCD
	lcd_init();
	// fixed21 checkpoint 4: normal lcd_init returned.
	bootdiag_pattern(0xF0); bootdiag_hold();
  
	// ---------------------------------------------------------------------
	// Init buttons
	init_buttons();

	// ---------------------------------------------------------------------
	// fixed26: Timer0_A0 IRQ-only diagnostic. Timer0_Init enables CCR0 IRQ.
	// Show a stable checkpoint first, clear any stale flag, then enable GIE.
	// The diagnostic TIMER0_A0 ISR in timer.c toggles raw LCD memory between
	// 0xAA and 0x55 on every interrupt and does nothing else.
	Timer0_Init();
	bootdiag_pattern(0x33);
	TA0CCTL0 &= ~CCIFG;
	TA0CCTL0 |= CCIE;
	__enable_interrupt();
	while (1)
	{
		__no_operation();
	}
	
}


// *************************************************************************************************
// @fn          init_global_variables
// @brief       Initialize global variables.
// @param       none
// @return      none
// *************************************************************************************************
void init_global_variables(void)
{
	// --------------------------------------------
	// Apply default settings

	// set menu pointers to default menu items
	ptrMenu_L1 = &menu_L1_Time;
//	ptrMenu_L1 = &menu_L1_Alarm;
//	ptrMenu_L1 = &menu_L1_Speed;
//	ptrMenu_L1 = &menu_L1_Temperature;
//	ptrMenu_L1 = &menu_L1_Altitude;
//	ptrMenu_L1 = &menu_L1_Acceleration;
	ptrMenu_L2 = &menu_L2_Date;
//	ptrMenu_L2 = &menu_L2_Stopwatch;
//	ptrMenu_L2 = &menu_L2_Rf;
//	ptrMenu_L2 = &menu_L2_Ppt;
//	ptrMenu_L2 = &menu_L2_Sync;
//	ptrMenu_L2 = &menu_L2_Distance;
//	ptrMenu_L2 = &menu_L2_Calories;
//	ptrMenu_L2 = &menu_L2_Battery;

	// Assign LINE1 and LINE2 display functions
	fptr_lcd_function_line1 = ptrMenu_L1->display_function;
	fptr_lcd_function_line2 = ptrMenu_L2->display_function;

	// Init system flags
	button.all_flags 	= 0;
	sys.all_flags 		= 0;
	request.all_flags 	= 0;
	display.all_flags 	= 0;
	message.all_flags	= 0;
	
	// Force full display update when starting up
	display.flag.full_update = 1;

#ifndef ISM_US
	// Use metric units when displaying values
	sys.flag.use_metric_units = 1;
#endif
	
	// Read calibration values from info memory
	read_calibration_values();
	
	// Set system time to default value
	reset_clock();
	
	// Set date to default value
	reset_date();
	
	// Set alarm time to default value 
	reset_alarm();
	
	// Set buzzer to default value
	reset_buzzer();
	
	// Reset stopwatch
	reset_stopwatch();
	
	// Reset altitude measurement
	reset_altitude_measurement();
	
	// Reset acceleration measurement
	reset_acceleration();
	
	// Reset temperature measurement 
	reset_temp_measurement();

	// Reset battery measurement
	reset_batt_measurement();
	battery_measurement();

  //Set cdtimer to a 5:00 minute default
  memcpy(scdtimer.defaultTime, "000500", sizeof(scdtimer.time));
  reset_cdtimer();
  
  // Reset random generator
  reset_random();
  
  // Reset agility measurement
  reset_agility();
  
  // Reset Unified Clock System reset handler
  void reset_ucs_reset_handler(void);
}


// *************************************************************************************************
// @fn          wakeup_event
// @brief       Process external / internal wakeup events.
// @param       none
// @return      none
// *************************************************************************************************
void wakeup_event(void)
{
	// Enable idle timeout
	sys.flag.idle_timeout_enabled = 1;

	// If buttons are locked, only display "buttons are locked" message
	if (button.all_flags && sys.flag.lock_buttons)
	{
		// Show "buttons are locked" message synchronously with next second tick
		if (!(BUTTON_NUM_IS_PRESSED && BUTTON_DOWN_IS_PRESSED))
		{
			message.flag.prepare     = 1;
			message.flag.type_locked = 1;
		}
		
		// Clear buttons
		button.all_flags = 0;	
	}
	// Process long button press event (while button is held)
	else if (button.flag.star_long)
	{
		// Clear button event
		button.flag.star_long = 0;

		// Call sub menu function
		ptrMenu_L1->mx_function(LINE1);

		// Set display update flag
		display.flag.full_update = 1;
	}
	else if (button.flag.num_long)
	{
		// Clear button event
		button.flag.num_long = 0;
		
		// Call sub menu function
		ptrMenu_L2->mx_function(LINE2);

		// Set display update flag
		display.flag.full_update = 1;	
	}
	// Process single button press event (after button was released)
	else if (button.all_flags)
	{
		// M1 button event ---------------------------------------------------------------------
		// (Short) Advance to next menu item
		if(button.flag.star) 
		{
			// Clean up display before activating next menu item 
			fptr_lcd_function_line1(LINE1, DISPLAY_LINE_CLEAR);
			
			// Go to next menu entry
			ptrMenu_L1 = ptrMenu_L1->next;
				
			// Assign new display function
			fptr_lcd_function_line1 = ptrMenu_L1->display_function;

			// Set Line1 display update flag
			display.flag.line1_full_update = 1;

			// Clear button flag
			button.flag.star = 0;
		}
		// NUM button event ---------------------------------------------------------------------
		// (Short) Advance to next menu item
		else if(button.flag.num) 
		{
			// Clean up display before activating next menu item 
			fptr_lcd_function_line2(LINE2, DISPLAY_LINE_CLEAR);

			// Go to next menu entry
			ptrMenu_L2 = ptrMenu_L2->next;

			// Assign new display function
			fptr_lcd_function_line2 = ptrMenu_L2->display_function;

			// Set Line2 display update flag
			display.flag.line2_full_update = 1;

			// Clear button flag
			button.flag.num = 0;
		}	
		// UP button event ---------------------------------------------------------------------
		// Activate user function for Line1 menu item
		else if(button.flag.up) 	
		{
			// Call direct function
			ptrMenu_L1->sx_function(LINE1);

			// Set Line1 display update flag
			display.flag.line1_full_update = 1;
	
			// Clear button flag	
			button.flag.up = 0;
		}			
		// DOWN button event ---------------------------------------------------------------------
		// Activate user function for Line2 menu item
		else if(button.flag.down) 	
		{
			// Call direct function
			ptrMenu_L2->sx_function(LINE2);

			// Set Line1 display update flag
			display.flag.line2_full_update = 1;
	
			// Clear button flag	
			button.flag.down = 0;
		}			
		// Backlight button event ---------------------------------------------------------------------
		else if(button.flag.backlight) 	
		{
             BlOnFlag = 1;
             // display_symbol(LCD_ICON_HEART, SEG_ON); 
             BUTTONS_OUT |= BUTTON_BACKLIGHT_PIN;
             BUTTONS_DIR |= BUTTON_BACKLIGHT_PIN;
             BUTTONS_DS  |= BUTTON_BACKLIGHT_PIN;
		     button.flag.backlight = 0;
 		}			
	}
	
	// Process internal events
	if (sys.all_flags)
	{
		// Idle timeout ---------------------------------------------------------------------
		if (sys.flag.idle_timeout)
		{
			// Clear timeout flag	
			sys.flag.idle_timeout = 0;	
			
			// Clear display
			clear_display();	

			// Set display update flags
			display.flag.full_update = 1;
		}
	}
	
	// Disable idle timeout
	sys.flag.idle_timeout_enabled = 0;
}


// *************************************************************************************************
// @fn          process_requests
// @brief       Process requested actions outside ISR context.
// @param       none
// @return      none
// *************************************************************************************************
void process_requests(void)
{
	// Do temperature measurement
	if (request.flag.temperature_measurement) temperature_measurement(FILTER_ON);
	
	// Do pressure measurement
  	if (request.flag.altitude_measurement) do_altitude_measurement(FILTER_ON);
	
	// Do acceleration measurement
	if (request.flag.acceleration_measurement) do_acceleration_measurement();
	
	// Do voltage measurement
	if (request.flag.voltage_measurement) battery_measurement();
	
	// Generate alarm (two signals every second)
	if (request.flag.buzzer) start_buzzer(2, BUZZER_ON_TICKS, BUZZER_OFF_TICKS);
	
	// Reset request flag
	request.all_flags = 0;
}


// *************************************************************************************************
// @fn          display_update
// @brief       Process display flags and call LCD update routines.
// @param       none
// @return      none
// *************************************************************************************************
void display_update(void)
{
	u8 line;
	u8 string[8];
	
	// ---------------------------------------------------------------------
	// Call Line1 display function
	if (display.flag.full_update ||	display.flag.line1_full_update)
	{
		clear_line(LINE1);	
		fptr_lcd_function_line1(LINE1, DISPLAY_LINE_UPDATE_FULL);
	}
	else if (ptrMenu_L1->display_update())
	{
		// Update line1 only when new data is available
		fptr_lcd_function_line1(LINE1, DISPLAY_LINE_UPDATE_PARTIAL);
	}


	// ---------------------------------------------------------------------
	// If message text should be displayed on Line2, skip normal update
	if (message.flag.show)
	{
		line = LINE2;
		
		// Select message to display
		if (message.flag.type_locked)			memcpy(string, "  LO?T", 6);
		else if (message.flag.type_unlocked)	memcpy(string, "  OPEN", 6);
		else if (message.flag.type_lobatt)		memcpy(string, "LOBATT", 6);
		else if (message.flag.type_alarm_on)	
		{
			memcpy(string, "  ON", 4);
			line = LINE1;
		}
		else if (message.flag.type_alarm_off)
		{
			memcpy(string, " OFF", 4);
			line = LINE1;
		}
		
		// Clear previous content
		clear_line(line);
		fptr_lcd_function_line2(line, DISPLAY_LINE_CLEAR);
		
		if (line == LINE2) 	display_chars(LCD_SEG_L2_5_0, string, SEG_ON);
		else 				display_chars(LCD_SEG_L1_3_0, string, SEG_ON);
		
		// Next second tick erases message and repaints original screen content
		message.all_flags = 0;
		message.flag.erase = 1;
	}
	// ---------------------------------------------------------------------
	// Call Line2 display function
	else if (display.flag.full_update || display.flag.line2_full_update)
	{
		clear_line(LINE2);
		fptr_lcd_function_line2(LINE2, DISPLAY_LINE_UPDATE_FULL);
	}
	else if (ptrMenu_L2->display_update() && !message.all_flags)
	{
		// Update line2 only when new data is available
		fptr_lcd_function_line2(LINE2, DISPLAY_LINE_UPDATE_PARTIAL);
	}
	
	// Clear display flag
	display.all_flags = 0;
}


// *************************************************************************************************
// @fn          to_lpm
// @brief       Go to LPM0/3. 
// @param       none
// @return      none
// *************************************************************************************************
void to_lpm(void)
{
	// Go to LPM3
	__bis_SR_register(LPM3_bits + GIE); 
	__no_operation();
}


// *************************************************************************************************
// @fn          idle_loop
// @brief       Go to LPM. Service watchdog timer when waking up.
// @param       none
// @return      none
// *************************************************************************************************
void idle_loop(void)
{
    /* fixed70:
       set_value() is a blocking foreground editor.  Putting it into LPM3 on
       every pass made the 200-ms button-repeat IRQ and contact IRQs become
       the scheduler, producing large UP/DOWN jumps and sluggish input.
       Keep the CPU awake while a set menu is active; normal UI idle can
       still use the guarded LPM3 block in main(). */
    if (!g_set_mode_active)
        to_lpm();

#ifdef USE_WATCHDOG
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif
}


// *************************************************************************************************
// @fn          read_calibration_values
// @brief       Read calibration values for temperature measurement, voltage measurement
//				from INFO memory.
// @param       none
// @return      none
// *************************************************************************************************
void read_calibration_values(void)
{
	u8 cal_data[CALIBRATION_DATA_LENGTH];		// Temporary storage for constants
	u8 i;
	u8 * flash_mem;         					// Memory pointer
	
	// Read calibration data from Info D memory
	flash_mem = (u8 *)0x1800;
	for (i=0; i<CALIBRATION_DATA_LENGTH; i++)
	{
		cal_data[i] = *flash_mem++;
	}
	
	if (cal_data[0] == 0xFF) 
	{
		// If no values are available (i.e. INFO D memory has been erased by user), assign experimentally derived values	
		sTemp.offset 	= -250;
		sBatt.offset 	= -10;	
		sAlt.altitude_offset	 = 0;
	}
	else
	{
		// Assign calibration data to global variables
sTemp.offset 	= (s16)((cal_data[2] << 8) + cal_data[3]);
		sBatt.offset 	= (s16)((cal_data[4] << 8) + cal_data[5]);
		// S/W version byte set during calibration?
		if (cal_data[12] != 0xFF)
		{
			sAlt.altitude_offset = (s16)((cal_data[10] << 8) + cal_data[11]);;
		}
		else
		{
			sAlt.altitude_offset = 0;	
		}
	}
}


