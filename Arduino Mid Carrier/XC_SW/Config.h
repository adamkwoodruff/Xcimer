#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


#define APIN_VOLTAGE_PROBE   PF_11  // Analog input for voltage sensing
#define APIN_CURRENT_PROBE   PA_6  // Analog input for current sensing
#define DPIN_WARN_LAMP_OUT   PF_8   // Digital output for warning lamp control
// Gate driver fault input and reset output
#define DPIN_GATE_FAULT      PE_10   // Active-low fault feedback
#define DPIN_GATE_RESET      PF_12   // Pulse high for driver reset
// Optional hardware enable input
#define DPIN_ENABLE_IN       PF_4   // External enable input
#define DPIN_CTRL_PS_DISABLE PF_6   // Power supply disable control (active high)
#define DPIN_CTRL_PWM_ENABLE PF_3   // PWM enable control (active high)
 

// IGBT PWM outputs
#define DPIN_IGBT_HS   PC_7   // High-side PWM single-ended input
#define DPIN_IGBT_LS   PA_9   // Low-side PWM single-ended input

// PWM parameters (defined in Config.cpp)
extern const float  IGBT_PWM_FREQ_HZ;          // e.g. 85000.0
extern const float  IGBT_PWM_MAX_DUTY;         // 0.45 (45%)
extern const uint8_t IGBT_PWM_RESOLUTION_BITS; // 12-bit 

// --- Dead Time & Clock Configuration ---
// IMPORTANT: Adjust DEAD_TIME based on IGBT/Driver specs.
const uint32_t IGBT_PWM_DEAD_TIME_NS = 500; // Example: 500 nanoseconds
// IMPORTANT: Verify this is the actual clock frequency supplied TO THE TIMER PERIPHERAL
const uint32_t PWM_TIMER_INPUT_CLOCK_FREQ = 200000000; // 200 MHz

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
extern const float PID_KP;                   // PID proportional gain
extern const float PID_KI;                   // PID integral gain
extern const float PID_KD;                   // PID derivative gain
extern const float PID_TO_DUTY_SCALE;        // Maps PID output to duty fraction
extern const float IGBT_PWM_MIN_DUTY;        // Minimum duty to keep gate off

// Centralized power state manager
#include "PowerState.h"


#endif // CONFIG_H