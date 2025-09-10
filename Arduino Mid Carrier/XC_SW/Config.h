#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Simple descriptor for digital pins with polarity metadata
struct PinDef {
    uint32_t pin;
    bool     active_high;
};

// --- Analog inputs ---
constexpr uint32_t APIN_CAP_VOLTAGE = PF_11; // A0 (J15-9)
constexpr uint32_t APIN_COIL_CURRENT = PA_6; // A1 (J15-11)
constexpr uint32_t APIN_SYSTEM_TEMP  = PF_12; // A2 (J15-13)

// --- Digital inputs ---
extern const PinDef IGBT_FAULT_IN;   // GPIO 5 (J14-42)  PE_10  active_high=false
extern const PinDef EXT_ENABLE_IN;   // GPIO 3 (J14-41)  PF_4   active_high=true

// --- PWM outputs ---
constexpr uint32_t PWM_IGBT_HI        = PC_7;  // PWM 0 (J15-25)
constexpr uint32_t PWM_MEAS_VOLTAGE   = PA_9;  // PWM 1 (DAC Voltage monitor)
constexpr uint32_t PWM_MEAS_CURRENT   = PA_10; // PWM 2 (DAC Current monitor)

// --- Digital outputs ---
extern const PinDef CHARGER_RELAY_CTRL;   // GPIO 4  active_high=false (inverted)
extern const PinDef DUMP_RELAY_CTRL;      // GPIO 6 (J14-40) PE_11 active_high=false
extern const PinDef DUMP_FAN_RELAY_CTRL;  // GPIO 2  active_high=true
extern const PinDef SCR_TRIG;             // GPIO 1 (J14-43) PF_3 active_high=true
extern const PinDef SCR_INHIBIT;          // PWM 3 (J15-31) PB_10 active_high=true
extern const PinDef WARN_LAMP;            // GPIO 0 (J14-39) PF_8 active_high=true

// --- PWM configuration ---
extern const float   IGBT_PWM_FREQ_HZ;          // e.g. 85000.0
extern const float   IGBT_PWM_MAX_DUTY;         // 0.45 (45%)
extern const uint8_t IGBT_PWM_RESOLUTION_BITS;  // 12-bit

// --- Calibration constants ---
extern const float VScale_V;
extern const float VOffset_V;
extern const float VScale_C;
extern const float VOffset_C;

// --- Debounce and warning lamp parameters ---
extern const unsigned long DEBOUNCE_DELAY_US;       // Debounce time in microseconds
extern const unsigned long WARN_BLINK_INTERVAL_MS;  // Blink half-period
extern const float WARN_VOLTAGE_THRESHOLD;          // Voltage threshold for warning lamp

// --- Control parameters ---
extern const float CURRENT_LIMIT_MAX;        // Maximum allowable set current
extern const float OVER_VOLTAGE_LIMIT;       // Disable current above this voltage
extern const float PID_KP;                   // PID proportional gain
extern const float PID_KI;                   // PID integral gain
extern const float PID_KD;                   // PID derivative gain
extern const float PID_TO_DUTY_SCALE;        // Maps PID output to duty fraction
extern const float IGBT_PWM_MIN_DUTY;        // Minimum duty to keep gate off

// --- Analog via PWM scaling ---
extern const float V_FULL_SCALE;  // full scale voltage for MEAS_VOLTAGE_OUT_PWM
extern const float I_FULL_SCALE;  // full scale current for MEAS_CURRENT_OUT_PWM

#endif // CONFIG_H

