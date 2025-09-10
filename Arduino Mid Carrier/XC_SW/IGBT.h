#ifndef IGBT_H
#define IGBT_H

#include "Config.h"

// Initialize PWM timers & pins
void init_igbt_pwm();

// Update PWM duty based on current_volt_set and current_curr_set
void update_igbt_pwm();

#endif // IGBT_H