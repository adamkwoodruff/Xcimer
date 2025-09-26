#include "CurrWaveform.h"
#include "Config.h"
#include "PowerState.h"
#include "SerialRPC.h"
#include <Arduino.h>

static inline float poly3(float A, float B, float C, float D, float s) {
    return ((D * s + C) * s + B) * s + A;
}

void update_curr_waveform(float dt_param) {
    static float    t = 0.0f;
    static bool     running = false;
    static bool     prevOutputEnabled = false;

    // Timebase (wrap-safe), throttle logs to ~20 Hz
    static uint32_t lastUs = 0;
    static uint32_t lastLogUs = 0;
    const uint32_t nowUs = micros();
    if (lastUs == 0) lastUs = nowUs; // first call init
    uint32_t dUs = nowUs - lastUs;   // wrap-safe unsigned diff
    lastUs = nowUs;

    // Build a robust dt (seconds)
    float dt_local = dUs * 1e-6f;            // from micros()
    float dt_eff   = dt_local;

    // If caller passes a sane dt (seconds), prefer it
    if (dt_param > 0.0f && dt_param < 0.2f) {
        dt_eff = dt_param;
    }

    // Clamp crazy steps (wraps, stalls, unit mix-ups)
    const bool dt_was_clamped = (dt_eff <= 0.0f || dt_eff > 0.2f);
    if (dt_was_clamped) dt_eff = 0.001f;     // 1 ms fallback

    const bool logTick = (uint32_t)(nowUs - lastLogUs) >= 50000UL; // 50 ms
    const bool outEn = PowerState::outputEnabled;
    char debugLine[512]; 
        snprintf(debugLine, sizeof(debugLine),
                "[CurrWave] START outEn=1  t1=%.3f th=%.3f t2=%.3f  "
                "A1=%.3f B1=%.3f C1=%.3f D1=%.3f  A2=%.3f B2=%.3f C2=%.3f D2=%.3f  "
                "dt_param=%.6f dt_local=%.6f dt_eff=%.6f (clamp=%d) dUs=%lu",
                (double)PowerState::currT1, (double)PowerState::currTHold, (double)PowerState::currT2,
                (double)PowerState::currA1, (double)PowerState::currB1, (double)PowerState::currC1, (double)PowerState::currD1,
                (double)PowerState::currA2, (double)PowerState::currB2, (double)PowerState::currC2, (double)PowerState::currD2,
                (double)dt_param, (double)dt_local, (double)dt_eff, dt_was_clamped ? 1 : 0, (unsigned long)dUs);
    SerialRPC.println(debugLine);

    // Rising-edge trigger on output enable
    if (outEn && !prevOutputEnabled && !running) {
        t = 0.0f;
        running = true;
    }

    // Abort on disable
    if (!outEn && running) {
        running = false;
        PowerState::setCurrent = 0.0f;
        PowerState::runCurrentWave = false;

        snprintf(debugLine, sizeof(debugLine),
                 "[CurrWave] ABORT outEn=0  t=%.6f  dt_param=%.6f dt_local=%.6f dt_eff=%.6f (clamp=%d) dUs=%lu",
                 (double)t, (double)dt_param, (double)dt_local, (double)dt_eff,
                 dt_was_clamped ? 1 : 0, (unsigned long)dUs);
        SerialRPC.println(debugLine);

        prevOutputEnabled = outEn;
        return;
    }

    // Status flag
    PowerState::runCurrentWave = running;

    if (!running) {
        if (logTick) {
            snprintf(debugLine, sizeof(debugLine),
                     "[CurrWave] IDLE outEn=%d set=%.3f  dt_param=%.6f dt_local=%.6f dt_eff=%.6f (clamp=%d) dUs=%lu",
                     outEn ? 1 : 0, (double)PowerState::setCurrent,
                     (double)dt_param, (double)dt_local, (double)dt_eff,
                     dt_was_clamped ? 1 : 0, (unsigned long)dUs);
            SerialRPC.println(debugLine);
            lastLogUs = nowUs;
        }
        PowerState::setCurrent = 0.0f;
        prevOutputEnabled = outEn;
        return;
    }

    // Segment timing
    float t1     = PowerState::currT1;     if (t1     < 0.0f) t1     = 0.0f;
    float t_hold = PowerState::currTHold;  if (t_hold < 0.0f) t_hold = 0.0f;
    float t2     = PowerState::currT2;     if (t2     < 0.0f) t2     = 0.0f;

    const float T2_EPS = 1e-6f;
    const float t2_eff = (t2 <= 0.0f) ? T2_EPS : t2;

    const float t2_start = t1 + t_hold;
    const float t_end    = t2_start + t2_eff;

    // Evaluate
    float y_raw = 0.0f;
    const char* phase = "RAMP_UP";

    if (t < t1) {
        const float s = (t1 > 0.0f) ? (t / t1) : 1.0f;
        y_raw = ((PowerState::currD1 * s + PowerState::currC1) * s + PowerState::currB1) * s + PowerState::currA1;
        phase = "RAMP_UP";
    } else if (t < t2_start) {
        y_raw = ((PowerState::currD1 * 1.0f + PowerState::currC1) * 1.0f + PowerState::currB1) * 1.0f + PowerState::currA1;
        phase = "HOLD";
    } else if (t < t_end) {
        const float s = (t - t2_start) / t2_eff;
        y_raw = ((PowerState::currD2 * s + PowerState::currC2) * s + PowerState::currB2) * s + PowerState::currA2;
        phase = "RAMP_DOWN";
    } else {
        running = false;
        PowerState::runCurrentWave = false;
        PowerState::setCurrent = 0.0f;

        snprintf(debugLine, sizeof(debugLine),
                 "[CurrWave] FINISH t=%.6f outEn=%d  dt_param=%.6f dt_local=%.6f dt_eff=%.6f (clamp=%d) dUs=%lu",
                 (double)t, outEn ? 1 : 0,
                 (double)dt_param, (double)dt_local, (double)dt_eff,
                 dt_was_clamped ? 1 : 0, (unsigned long)dUs);
        SerialRPC.println(debugLine);

        prevOutputEnabled = outEn;
        return;
    }

    // Clamp & publish
    float y = y_raw;
    if (y < 0.0f) y = 0.0f;
    if (y > CURRENT_LIMIT_MAX) y = CURRENT_LIMIT_MAX;
    PowerState::setCurrent = y;

    // Telemetry
    if (logTick) {
        snprintf(debugLine, sizeof(debugLine),
                 "[CurrWave] %s t=%.6f dt_eff=%.6f (param=%.6f local=%.6f clamp=%d) "
                 "outEn=%d y_raw=%.3f y_set=%.3f run=%d",
                 phase, (double)t, (double)dt_eff, (double)dt_param, (double)dt_local,
                 dt_was_clamped ? 1 : 0, outEn ? 1 : 0,
                 (double)y_raw, (double)y, running ? 1 : 0);
        SerialRPC.println(debugLine);
        lastLogUs = nowUs;
    }

    // Advance time
    t += dt_eff;

    prevOutputEnabled = outEn;
}
