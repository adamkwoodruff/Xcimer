#include "CurrWaveform.h"
#include "Config.h"
#include "PowerState.h"
#include <Arduino.h> // Required for Serial communication

static inline float poly3(float A, float B, float C, float D, float s) {
    return ((D * s + C) * s + B) * s + A;
}

void update_curr_waveform(float dt) { 
    Serial.print("Called 1");
    static float t = 0.0f;
    static bool running = false;

    // Trigger condition: Check if a new waveform run is requested
    if (PowerState::runCurrentWave && !running) { 
       Serial.print("Called 2");
        t = 0.0f;
        running = true;
        PowerState::runCurrentWave = false; // Latch the start signal
        Serial.println("[RPC DEBUG] Waveform triggered. Resetting time t=0.0");
    }

    if (!running) {
        return; 
        Serial.print("Called 3");
    }

    // --- Prepare variables for the single print statement ---
    const char* phase_str;
    float s = -1.0f; // Use -1.0f to indicate 'not applicable' for the hold phase
    float y_raw;     // The raw calculated current before clamping

    // Load time constants from PowerState
    const float t1 = PowerState::currT1;
    const float t_hold = PowerState::currTHold;
    const float t2 = PowerState::currT2;
    const float t2_start = t1 + t_hold;
    const float t_end = t2_start + t2;

    if (t < t1 && t1 > 0.0f) { 
        Serial.print("Called 4");
        phase_str = "Ramp Up";
        s = t / t1;
        y_raw = poly3(PowerState::currA1, PowerState::currB1, PowerState::currC1, PowerState::currD1, s);
    } else if (t < t2_start) { 
        Serial.print("Called 5");
        phase_str = "Hold";
        y_raw = poly3(PowerState::currA1, PowerState::currB1, PowerState::currC1, PowerState::currD1, 1.0f);
    } else if (t < t_end && t2 > 0.0f) { 
        Serial.print("Called 6");
        phase_str = "Ramp Down";
        s = (t - t2_start) / t2;
        y_raw = poly3(PowerState::currA2, PowerState::currB2, PowerState::currC2, PowerState::currD2, s);
    } else {
        phase_str = "Finished";
        y_raw = poly3(PowerState::currA2, PowerState::currB2, PowerState::currC2, PowerState::currD2, 1.0f);
        running = false;
    }

    float y_clamped = y_raw;
    if (y_clamped < 0.0f) y_clamped = 0.0f;
    if (y_clamped > CURRENT_LIMIT_MAX) y_clamped = CURRENT_LIMIT_MAX;

    PowerState::setCurrent = y_clamped;

    // --- CORRECTED PRINT SECTION ---
    // We build the line piece-by-piece since SerialRPC doesn't have .printf()
    Serial.print("[RPC] t:");
    Serial.print(t, 3); // The '3' prints the float with 3 decimal places
    Serial.print(" | Phase:");
    Serial.print(phase_str);
    Serial.print(" | s:");
    Serial.print(s, 3);
    Serial.print(" | y_raw:");
    Serial.print(y_raw, 3);
    Serial.print(" | I_set:");
    Serial.println(PowerState::setCurrent, 3); // Use .println() for the final part to add a newline
    // --- END OF FIX ---

    t += dt;
}