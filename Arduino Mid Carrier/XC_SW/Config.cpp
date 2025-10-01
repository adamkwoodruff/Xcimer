#include "Config.h"
#include "PowerState.h"//

// --- Define Calibration Constants ---
//
// initially bringing the system up.
float VScale_V = 0.0f;
float VOffset_V = 0.0f;
float VScale_C = 0.0f;
float VOffset_C = 0.0f;

float VOLTAGE_PWM_FULL_SCALE = 400.0f; // Default full-scale voltage for measured PWM output

// --- Define Timing/Threshold Constants ---
unsigned long DEBOUNCE_DELAY_US = 1000;
unsigned long WARN_BLINK_INTERVAL_MS = 5000;
float WARN_VOLTAGE_THRESHOLD = 50.0f;

// --- Control Parameters ---
float CURRENT_LIMIT_MAX  = 3600.0f;   // Max current in milli amps
float OVER_VOLTAGE_LIMIT = 285.0f;  // Disable if output exceeds this


// --- IGBT PWM configuration ---
float  IGBT_PWM_FREQ_HZ = 500.0f;; // Global PWM frequency (Hz)
const uint8_t IGBT_PWM_RESOLUTION_BITS = 12;      // 12-bit resolution 

// Load model / IGBT guard rails
float MIN_LOAD_RES_OHM   = 0.010f;  // Ω
float IGBT_MIN_DUTY_PCT  = 5.0f;    // %
float IGBT_MAX_DUTY_PCT  = 95.0f;   // %
