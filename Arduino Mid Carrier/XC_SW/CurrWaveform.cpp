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
        // FIX 1: Consume the trigger immediately so it can be re-triggered later.
        
        Serial.println("[RPC DEBUG] Waveform triggered. Resetting time t=0.0");
    }

    if (!running) {
        // If not running, ensure the set current is zero and do nothing else.
        PowerState::setCurrent = 0.0f;
        return;
    }

    float y_raw = 0.0f;
    float A, B, C, D;

    const float t1 = PowerState::currT1;
    const float t_hold = PowerState::currTHold;
    const float t2 = PowerState::currT2;
    const float t2_start = t1 + t_hold;
    const float t_end = t2_start + t2;

    if (t < t2_start) { // Ramp Up and Hold phases
        A = PowerState::currA1; B = PowerState::currB1; C = PowerState::currC1; D = PowerState::currD1;
        if (t < t1 && t1 > 0.0f) {
            float s = t / t1;
            y_raw = poly3(A, B, C, D, s);
        } else {
            y_raw = poly3(A, B, C, D, 1.0f);
        }
    } else if (t < t_end && t2 > 0.0f) { // Ramp Down phase
        A = PowerState::currA2; B = PowerState::currB2; C = PowerState::currC2; D = PowerState::currD2;
        float s = (t - t2_start) / t2;
        y_raw = poly3(A, B, C, D, s);
    } else { // Finished
        // The sequence is over, stop running. The current will be set to 0 below.
        running = false; 
        PowerState::runCurrentWave = false;
    }

    // FIX 2: If the waveform just finished, set current to 0. Otherwise, use the calculated value.
    if (!running) { 
        PowerState::setCurrent = 0.0f;
        Serial.println("[RPC DEBUG] Waveform finished. Setting current to 0.");
    } else {
        // Clamp the calculated value while running
        float y_clamped = y_raw;
        if (y_clamped < 0.0f) y_clamped = 0.0f;
        if (y_clamped > CURRENT_LIMIT_MAX) y_clamped = CURRENT_LIMIT_MAX;
        PowerState::setCurrent = y_clamped;
    }

    t += dt;
}