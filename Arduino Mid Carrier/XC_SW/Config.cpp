#include "Config.h"
#include "PowerState.h"

// --- Define Calibration Constants ---
//
// initially bringing the system up.
const float VScale_V = -606.0606f;   // volts per volt
const float VOffset_V =27.0f;
const float VScale_C = 5.0f * 1.0f;   // 5.0 is the resistor, and 1.0 is an arbitrary value
const float VOffset_C = 0.0f;

// --- Define Timing/Threshold Constants ---
const unsigned long DEBOUNCE_DELAY_US = 1000;
const unsigned long WARN_BLINK_INTERVAL_MS = 5000;
const float WARN_VOLTAGE_THRESHOLD = 50.0;

// --- Control Parameters ---
const float CURRENT_LIMIT_MAX  = 3000.0f;   // Max current in milli amps
const float OVER_VOLTAGE_LIMIT = 285.0f;  // Disable if output exceeds this


// --- IGBT PWM configuration ---
const float  IGBT_PWM_FREQ_HZ          = 500.0f; // Global PWM frequency (Hz)
const uint8_t IGBT_PWM_RESOLUTION_BITS = 12;      // 12-bit resolution
