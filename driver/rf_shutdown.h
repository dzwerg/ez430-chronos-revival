#ifndef RF_SHUTDOWN_H_
#define RF_SHUTDOWN_H_

#include "project.h"

/* Result of the one-shot defensive RF shutdown performed during boot. */
extern volatile u8 rf_shutdown_result;
extern volatile u8 rf_shutdown_idle_status;
extern volatile u8 rf_shutdown_sleep_status;
extern volatile u8 rf_shutdown_error;

void rf_force_powerdown(void);

#endif
