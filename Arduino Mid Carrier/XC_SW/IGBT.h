#ifndef IGBT_H
#define IGBT_H


#include <Arduino.h>
#include "Config.h"
#include "PowerState.h"


// Initialize IGBT HI PWM (center‑aligned on TIM3 CH2 / PC7)
void init_igbt();


// Update IGBT HI PWM with clamping and fault inhibit
void update_igbt();


// Read the (active‑low) IGBT gate fault input
bool igbt_fault_active();


// NEW: true when the actual gate drive pin is logic LOW
bool igbt_drive_is_low();


// NEW: expose a one‑time setup that makes TIM3 publish OC2REF on TRGO
bool igbt_enable_trgo_from_pwm();


#endif // IGBT_H