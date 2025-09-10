#ifndef POWERSTATE_H
#define POWERSTATE_H

#include "Config.h"

struct PowerState {
    static volatile float setVoltage;
    static volatile float setCurrent;
    static volatile float probeVoltageOutput;
    static volatile float probeCurrent;

    // Enable logic
    static volatile bool internalEnable;   // Set via UI or logic
    static volatile bool externalEnable;   // Sampled from digital input
    static volatile bool outputEnabled;    // True only when both enables are HIGH

    // Warning lamp
    static volatile bool warnLampTestState;
    static unsigned long lastWarnBlinkTimeMs;
    static bool warnLampOn;
    static volatile bool faultLockout;
   
};

#endif // POWERSTATE_H
