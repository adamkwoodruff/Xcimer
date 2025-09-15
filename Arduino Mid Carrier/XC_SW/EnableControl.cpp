#include "EnableControl.h"
#include "PowerState.h"

void init_enable_control() {
  pinMode(DPIN_ENABLE_IN, HW_INPUT_PIN_MODE);    // External enable input
  // Initialize control outputs to match stored state
  pinMode(DPIN_WARN_LAMP_OUT, OUTPUT);           // Warning lamp output
  digitalWrite(DPIN_WARN_LAMP_OUT, HIGH); 

  pinMode(DPIN_DUMP_FAN, OUTPUT);
  pinMode(DPIN_DUMP_RELAY, OUTPUT);
  pinMode(DPIN_CHARGER_RELAY, OUTPUT);

  digitalWrite(DPIN_DUMP_FAN, LOW);
  digitalWrite(DPIN_DUMP_RELAY, HIGH);
  digitalWrite(DPIN_CHARGER_RELAY, HIGH); 

  pinMode(DPIN_SCR_TRIG, OUTPUT);
  pinMode(DPIN_SCR_INHIB, OUTPUT);

  digitalWrite(DPIN_SCR_TRIG, HIGH);
  digitalWrite(DPIN_SCR_INHIB, LOW);

}

void update_enable_inputs() {
  // Sample external enable input (debounce if needed)
  PowerState::externalEnable = (digitalRead(DPIN_ENABLE_IN) == HW_INPUT_ACTIVE_STATE);

  // Combined logic: both internal and external must be true
  PowerState::outputEnabled = PowerState::externalEnable && PowerState::internalEnable;
}

void update_enable_outputs() {
  unsigned long now_ms = millis();

  if (PowerState::probeVoltageOutput >= WARN_VOLTAGE_THRESHOLD) {
    // Blink the warning lamp when the output voltage is above the threshold
    if (now_ms - PowerState::lastWarnBlinkTimeMs >= WARN_BLINK_INTERVAL_MS) {
      PowerState::lastWarnBlinkTimeMs = now_ms;
      PowerState::warnLampOn = !PowerState::warnLampOn;
      digitalWrite(DPIN_WARN_LAMP_OUT, PowerState::warnLampOn ? HIGH : LOW); // does not need to be changed
    }

  } else if (PowerState::warnLampTestState) {
    // Test button pressed - force the lamp on
    PowerState::warnLampOn = true;
    digitalWrite(DPIN_WARN_LAMP_OUT, LOW);

  } else {
    // Neither condition active - ensure the lamp stays off
    PowerState::warnLampOn = false;
    digitalWrite(DPIN_WARN_LAMP_OUT, HIGH);
  } 

  digitalWrite(DPIN_DUMP_FAN,      PowerState::DumpFan      ? LOW : HIGH);
  digitalWrite(DPIN_DUMP_RELAY,    PowerState::DumpRelay    ? LOW : HIGH);
  digitalWrite(DPIN_CHARGER_RELAY, PowerState::ChargerRelay ? LOW : HIGH); 
  digitalWrite(DPIN_SCR_TRIG,      PowerState::ScrTrig      ? LOW : HIGH);
  digitalWrite(DPIN_SCR_INHIB,     PowerState::ScrInhib     ? LOW : HIGH);

}

int get_output_enable_state() {
  return PowerState::outputEnabled ? 1 : 0;
} 

int dump_fan(int state) {
  digitalWrite(DPIN_DUMP_FAN, (state != 0) ? LOW : HIGH);
  return 1;
}

int dump_relay(int state) {
  digitalWrite(DPIN_DUMP_RELAY, (state != 0) ? LOW : HIGH);
  return 1;
}

int charger_relay(int state) {
  digitalWrite(DPIN_CHARGER_RELAY, (state != 0) ? LOW : HIGH);
  return 1;
} 

int scr_trig(int state) {
    return PowerState::ScrTrig ? 1 : 0;
}
int scr_inhib(int state) {
    return PowerState::ScrInhib ? 1 : 0;
}
