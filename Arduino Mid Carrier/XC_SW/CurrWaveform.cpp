#include "CurrWaveform.h"
#include "Config.h"
#include "PowerState.h"
#include <Arduino.h> // Required for Serial communication
#include <stdio.h>   // Required for snprintf

static inline float poly3(float A, float B, float C, float D, float s) {
    return ((D * s + C) * s + B) * s + A;
}

void update_curr_waveform(float dt) {
    static float t = 0.0f;
    static bool running = false;

    // Trigger condition: Check if a new waveform run is requested
    if (PowerState::runCurrentWave && !running) {
        t = 0.0f;
        running = true;
        PowerState::runCurrentWave = false; // Latch the start signal
        Serial.println("[RPC DEBUG] Waveform triggered. Resetting time t=0.0");
    }

    if (!running) {
        return;
    }

    // --- Prepare variables for the single print statement ---
    const char* phase_str;
    float s = -1.0f; // Use -1.0f to indicate 'not applicable' for the hold phase
    float y_raw;     // The raw calculated current before clamping
    float A, B, C, D; // Local variables to hold the active coefficients

    // Load time constants from PowerState
    const float t1 = PowerState::currT1;
    const float t_hold = PowerState::currTHold;
    const float t2 = PowerState::currT2;
    const float t2_start = t1 + t_hold;
    const float t_end = t2_start + t2;

    if (t < t2_start) { // This block covers both "Ramp Up" and "Hold" phases
        // Use the first set of polynomial coefficients
        A = PowerState::currA1;
        B = PowerState::currB1;
        C = PowerState::currC1;
        D = PowerState::currD1;
        if (t < t1 && t1 > 0.0f) {
            phase_str = "Ramp Up";
            s = t / t1;
            y_raw = poly3(A, B, C, D, s);
        } else {
            phase_str = "Hold";
            y_raw = poly3(A, B, C, D, 1.0f);
        }
    } else { // This block covers "Ramp Down" and "Finished" phases
        // Use the second set of polynomial coefficients
        A = PowerState::currA2;
        B = PowerState::currB2;
        C = PowerState::currC2;
        D = PowerState::currD2;
        if (t < t_end && t2 > 0.0f) {
            phase_str = "Ramp Down";
            s = (t - t2_start) / t2;
            y_raw = poly3(A, B, C, D, s);
        } else {
            phase_str = "Finished";
            y_raw = poly3(A, B, C, D, 1.0f);
            running = false;
        }
    }

    float y_clamped = y_raw;
    if (y_clamped < 0.0f) y_clamped = 0.0f;
    if (y_clamped > CURRENT_LIMIT_MAX) y_clamped = CURRENT_LIMIT_MAX;

    PowerState::setCurrent = y_clamped;

    // --- UPDATED PRINT SECTION ---
    char debug_buffer[200]; // Increased buffer size for the extra values

    snprintf(debug_buffer, sizeof(debug_buffer),
             "[RPC] t:%.3f|Phase:%-10s|s:%.3f|y:%.3f|I:%.3f | Coeffs[A:%.2f,B:%.2f,C:%.2f,D:%.2f]",
             t, phase_str, s, y_raw, PowerState::setCurrent, A, B, C, D);

    Serial.println(debug_buffer);
    // --- END OF UPDATE ---

    t += dt;
}