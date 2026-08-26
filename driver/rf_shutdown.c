#include "project.h"
#include "rf_shutdown.h"

#define RF_WAIT_LIMIT 60000u

volatile u8 rf_shutdown_result;
volatile u8 rf_shutdown_idle_status;
volatile u8 rf_shutdown_sleep_status;
volatile u8 rf_shutdown_error;

static u8 rf_strobe_with_timeout(u8 command, u8 *status)
{
    u16 timeout = RF_WAIT_LIMIT;

    while (!(RF1AIFCTL1 & RFINSTRIFG) && --timeout) { }
    if (!timeout) return 0;

    RF1AIFCTL1 &= (u16)~RFSTATIFG;
    RF1AINSTRB = command;

    timeout = RF_WAIT_LIMIT;
    while (!(RF1AIFCTL1 & RFSTATIFG) && --timeout) { }
    if (!timeout) return 0;

    *status = RF1ASTATB;
    return 1;
}

void rf_force_powerdown(void)
{
    u8 ok_idle;
    u8 ok_sleep;

    /* No RF stack is linked.  This is only a defensive one-shot shutdown:
       disable every RF interrupt, leave RX/TX, then enter power-down. */
    RF1AIE = 0;
    RF1AIFG = 0;
    RF1AIFERR = 0;

    ok_idle = rf_strobe_with_timeout(RF_SIDLE, (u8 *)&rf_shutdown_idle_status);
    ok_sleep = rf_strobe_with_timeout(RF_SPWD, (u8 *)&rf_shutdown_sleep_status);

    rf_shutdown_error = (u8)RF1AIFERR;
    rf_shutdown_result = (ok_idle ? BIT0 : 0u) |
                         (ok_sleep ? BIT1 : 0u) |
                         ((UCSCTL6 & XT2OFF) ? BIT2 : 0u);

    /* XT2 is the RF crystal.  No remaining feature is allowed to request it. */
    UCSCTL6 |= XT2OFF;
    rf_shutdown_result |= BIT2;
}
