#include "CurrWaveform.h"
#include "Config.h"
#include "PowerState.h"

static inline float poly3(float A, float B, float C, float D, float s) {
    return ((D * s + C) * s + B) * s + A;
}

void update_curr_waveform(float dt) {
    static float t = 0.0f;
    static bool running = false;

    if (PowerState::runCurrentWave && !running) {
        t = 0.0f;
        running = true;
        PowerState::runCurrentWave = false; // latch start
    }
    if (!running) {
        return;
    }

    const float t1 = PowerState::currT1;
    const float t_hold = PowerState::currTHold;
    const float t2 = PowerState::currT2;
    const float t2_start = t1 + t_hold;
    const float t_end = t2_start + t2;
    float y;

    if (t < t1 && t1 > 0.0f) {
        float s = t / t1;
        y = poly3(PowerState::currA1, PowerState::currB1, PowerState::currC1, PowerState::currD1, s);
    } else if (t < t2_start) {
        y = poly3(PowerState::currA1, PowerState::currB1, PowerState::currC1, PowerState::currD1, 1.0f);
    } else if (t < t_end && t2 > 0.0f) {
        float s = (t - t2_start) / t2;
        y = poly3(PowerState::currA2, PowerState::currB2, PowerState::currC2, PowerState::currD2, s);
    } else {
        y = poly3(PowerState::currA2, PowerState::currB2, PowerState::currC2, PowerState::currD2, 1.0f);
        running = false;
    }

    if (y < 0.0f) y = 0.0f;
    if (y > CURRENT_LIMIT_MAX) y = CURRENT_LIMIT_MAX;
    PowerState::setCurrent = y;
    t += dt;
}
