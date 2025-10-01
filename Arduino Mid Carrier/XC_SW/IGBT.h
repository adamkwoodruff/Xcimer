#ifndef IGBT_H
#define IGBT_H

#include <Arduino.h>
#include "Config.h"
#include "PowerState.h"

// Initialize IGBT HI PWM (500 Hz)
void init_igbt();

// Update IGBT HI PWM with clamping and fault inhibit
void update_igbt();

// Read the (active-low) IGBT gate fault input
bool igbt_fault_active(); 
//void update_igbt_ignore()

#endif // IGBT_H