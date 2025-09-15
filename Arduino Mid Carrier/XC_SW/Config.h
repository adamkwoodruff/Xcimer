#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


#define APIN_VOLTAGE_PROBE   PF_11  // Analog input for voltage sensing
#define APIN_CURRENT_PROBE   PA_6  // Analog input for current sensing
#define DPIN_WARN_LAMP_OUT   PF_8   // Digital output for warning lamp control
// Gate driver fault input and reset output
#define DPIN_GATE_FAULT      PE_10   // Active-low fault feedback

// Optional hardware enable input
#define DPIN_ENABLE_IN       PF_4   // External enable input

#define DPIN_DUMP_FAN       PF_3 //GPIO 2 
#define DPIN_DUMP_RELAY     PE_11 //GPIO 6
#define DPIN_CHARGER_RELAY  PF_12 //GPIO 4 

#define DPIN_SCR_TRIG  PF_6 //GPIO 1 
#define DPIN_SCR_INHIB  PB_10 //PWM 3 

#define MEASURED_VOLT_OUT  PA_9 //PWM 1 
#define MEASURED_CURR_OUT  PA_10 //PWM 2 

// IGBT PWM outputs
#define DPIN_IGBT_HS   PC_7   // PWM 0 High-side PWM single-ended input


// PWM parameters (defined in Config.cpp)
extern const float  IGBT_PWM_FREQ_HZ;          // e.g. 85000.0
extern const uint8_t IGBT_PWM_RESOLUTION_BITS; // 12-bit 


// --- Input Logic (Adjust if using active-low sensors) ---
#define HW_INPUT_ACTIVE_STATE HIGH
#define HW_INPUT_PIN_MODE     INPUT_PULLDOWN // Or INPUT_PULLUP if active-low

// --- Calibration Constants (!!! REPLACE WITH ACTUAL VALUES !!!) ---
extern const float VScale_V;
extern const float VOffset_V;
extern const float VScale_C;
extern const float VOffset_C;

// --- Debounce Parameters ---
extern const unsigned long DEBOUNCE_DELAY_US; // Debounce time in microseconds

// --- Warning Lamp Parameters ---
extern const unsigned long WARN_BLINK_INTERVAL_MS; // Blink half-period
extern const float WARN_VOLTAGE_THRESHOLD;      // Voltage threshold for warning

// --- Control Parameters ---
extern const float CURRENT_LIMIT_MAX;        // Maximum allowable set current
extern const float OVER_VOLTAGE_LIMIT;       // Disable current above this voltage


// Centralized power state manager
#include "PowerState.h"


#endif // CONFIG_H