#include "EnableControl.h"
#include "PowerState.h"
#include "Config.h"

static inline void writePin(const PinDef &def, bool state) {
    digitalWrite(def.pin, (state == def.active_high) ? HIGH : LOW);
}

void init_enable_control() {
    // Configure digital inputs
    pinMode(IGBT_FAULT_IN.pin, IGBT_FAULT_IN.active_high ? INPUT_PULLDOWN : INPUT_PULLUP);
    pinMode(EXT_ENABLE_IN.pin, EXT_ENABLE_IN.active_high ? INPUT_PULLDOWN : INPUT_PULLUP);

    // Configure digital outputs with safe states
    pinMode(CHARGER_RELAY_CTRL.pin, OUTPUT);
    pinMode(DUMP_RELAY_CTRL.pin, OUTPUT);
    pinMode(DUMP_FAN_RELAY_CTRL.pin, OUTPUT);
    pinMode(SCR_TRIG.pin, OUTPUT);
    pinMode(SCR_INHIBIT.pin, OUTPUT);
    pinMode(WARN_LAMP.pin, OUTPUT);

    // Safe states at boot
    writePin(CHARGER_RELAY_CTRL, false);
    writePin(DUMP_RELAY_CTRL, false);
    writePin(DUMP_FAN_RELAY_CTRL, false);
    writePin(SCR_TRIG, false);
    writePin(SCR_INHIBIT, true);   // inhibit active
    writePin(WARN_LAMP, false);

    // PWM outputs start at 0 duty
    pinMode(PWM_IGBT_HI, OUTPUT);
    pinMode(PWM_MEAS_VOLTAGE, OUTPUT);
    pinMode(PWM_MEAS_CURRENT, OUTPUT);
    analogWrite(PWM_IGBT_HI, 0);
    analogWrite(PWM_MEAS_VOLTAGE, 0);
    analogWrite(PWM_MEAS_CURRENT, 0);
}

void update_enable_inputs() {
    PowerState::igbtFault = (digitalRead(IGBT_FAULT_IN.pin) == (IGBT_FAULT_IN.active_high ? HIGH : LOW));
    PowerState::extEnable = (digitalRead(EXT_ENABLE_IN.pin) == (EXT_ENABLE_IN.active_high ? HIGH : LOW));
}

void update_enable_outputs() {
    // Warning lamp uses PowerState::warnLamp directly
    writePin(CHARGER_RELAY_CTRL, PowerState::chargerRelay);
    writePin(DUMP_RELAY_CTRL, PowerState::dumpRelay);
    writePin(DUMP_FAN_RELAY_CTRL, PowerState::dumpFan);
    writePin(WARN_LAMP, PowerState::warnLamp);

    // SCR inhibit: high = inhibit. Allow low only when scrInhibit_allow true.
    writePin(SCR_INHIBIT, !PowerState::scrInhibit_allow);

    // SCR trigger pulse generation
    static unsigned long pulseEndUs = 0;
    unsigned long nowUs = micros();
    if (PowerState::scrTrig_cmd) {
        writePin(SCR_TRIG, true);
        pulseEndUs = nowUs + 10; // 10us pulse
        PowerState::scrTrig_cmd = false;
    }
    if (pulseEndUs && nowUs > pulseEndUs) {
        writePin(SCR_TRIG, false);
        pulseEndUs = 0;
    }

    // Update PWM-DAC outputs based on duty (0..1)
    uint32_t maxCount = (1u << IGBT_PWM_RESOLUTION_BITS) - 1u;
    analogWrite(PWM_MEAS_VOLTAGE, (uint32_t)(PowerState::measVoltagePwmDuty * maxCount));
    analogWrite(PWM_MEAS_CURRENT, (uint32_t)(PowerState::measCurrentPwmDuty * maxCount));
}

