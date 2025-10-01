#include "CurrWaveform.h"
#include "Config.h"
#include "PowerState.h"
#include <Arduino.h>

// cubic helper
static inline float poly3(float A, float B, float C, float D, float s) {
    return ((D * s + C) * s + B) * s + A;
}

void update_curr_waveform(float dt) {
    static float t = 0.0f;
    static bool running = false;
    static bool prevOutputEnabled = false;
    static bool prevChargeRelayOn = false;

    const bool outEn = PowerState::outputEnabled;
    const bool chargeRelayOn = PowerState::ChargerRelay;

    // Rising-edge trigger on external-enabled output, but only when the charge
    // relay is OFF. Also allow starting when the charge relay transitions from
    // ON→OFF while the output remains enabled.
    const bool chargeRelayJustDisabled = prevChargeRelayOn && !chargeRelayOn;
    if (outEn && !chargeRelayOn && !running &&
        (!prevOutputEnabled || chargeRelayJustDisabled)) {
        t = 0.0f;
        running = true;
    }

    // Abort immediately if output is disabled mid-run
    // or if the charge relay turns ON while a waveform is executing.
    if ((!outEn || chargeRelayOn) && running) {
        running = false;
        PowerState::setCurrent = 0.0f;
        PowerState::runCurrentWave = false; // status only
        prevOutputEnabled = outEn;
        prevChargeRelayOn = chargeRelayOn;
        return;
    }

    // Publish status flag (status, not a control)
    PowerState::runCurrentWave = running;

    if (!running) {
        PowerState::setCurrent = 0.0f;
        prevOutputEnabled = outEn;
        prevChargeRelayOn = chargeRelayOn;
        return;
    }

    // --- segment timing ---
    float t1     = PowerState::currT1;     if (t1     < 0.0f) t1     = 0.0f;
    float t_hold = PowerState::currTHold;  if (t_hold < 0.0f) t_hold = 0.0f;
    float t2     = PowerState::currT2;     if (t2     < 0.0f) t2     = 0.0f;

    const float T2_EPS = 1e-6f;                  // ensure we enter ramp-down branch
    const float t2_eff = (t2 <= 0.0f) ? T2_EPS : t2;

    const float t2_start = t1 + t_hold;
    const float t_end    = t2_start + t2_eff;

    // --- evaluate waveform (NO locals named D1/D2!) ---
    float y_raw = 0.0f;

    if (t < t1) {
        const float s = (t1 > 0.0f) ? (t / t1) : 1.0f;
        y_raw = poly3(PowerState::currA1, PowerState::currB1,
                      PowerState::currC1, PowerState::currD1, s);
    } else if (t < t2_start) {
        // Hold: end value of the first polynomial
        y_raw = poly3(PowerState::currA1, PowerState::currB1,
                      PowerState::currC1, PowerState::currD1, 1.0f);
    } else if (t < t_end) {
        const float s = (t - t2_start) / t2_eff;
        y_raw = poly3(PowerState::currA2, PowerState::currB2,
                      PowerState::currC2, PowerState::currD2, s);
    } else {
        // Finished
        running = false;
        PowerState::runCurrentWave = false; // status
        PowerState::setCurrent = 0.0f;
        prevOutputEnabled = outEn;
        prevChargeRelayOn = chargeRelayOn;
        return;
    }

    // Clamp and publish
    float y = y_raw;
    if (y < 0.0f) y = 0.0f;
    if (y > CURRENT_LIMIT_MAX) y = CURRENT_LIMIT_MAX;
    PowerState::setCurrent = y;

    // Advance time base
    if (dt < 0.0f) dt = 0.0f;
    t += dt;
    prevOutputEnabled = outEn;
    prevChargeRelayOn = chargeRelayOn;
}

