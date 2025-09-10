#ifndef POWERSTATE_H
#define POWERSTATE_H

#include "Config.h"

// Centralized container for system run-time state and commands.
struct PowerState {
    // Setpoints and measured values
    static volatile float setVoltage;
    static volatile float setCurrent;
    static volatile float probeVoltageOutput;
    static volatile float probeCurrent;

    // Digital inputs
    static volatile bool igbtFault;   // true = fault asserted
    static volatile bool extEnable;   // external enable line state

    // Commanded outputs
    static volatile bool chargerRelay;
    static volatile bool dumpRelay;
    static volatile bool dumpFan;
    static volatile bool warnLamp;
    static volatile bool scrTrig_cmd;       // edge command
    static volatile bool scrInhibit_allow;  // when true, SCR_INHIBIT can be driven LOW

    // Telemetry of PWM-DAC outputs (0.0-1.0 duty)
    static volatile float measVoltagePwmDuty;
    static volatile float measCurrentPwmDuty;
};

#endif // POWERSTATE_H

