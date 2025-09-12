#include "EnableControl.h"
#include "PowerState.h"

void init_enable_control() {
  pinMode(DPIN_ENABLE_IN, HW_INPUT_PIN_MODE);    // External enable input
  // Initialize control outputs to match stored state
  pinMode(DPIN_WARN_LAMP_OUT, OUTPUT);           // Warning lamp output
  digitalWrite(DPIN_WARN_LAMP_OUT, HIGH); 

  pinMode(DUMP_FAN, OUTPUT);
  pinMode(DUMP_RELAY, OUTPUT);
  pinMode(CHARGER_RELAY, OUTPUT);

  digitalWrite(DUMP_FAN, LOW);
  digitalWrite(DUMP_RELAY, LOW);
  digitalWrite(CHARGER_RELAY, LOW); 

  pinMode(SCR_TRIG, OUTPUT);
  pinMode(SCR_INHIB, OUTPUT);

  digitalWrite(SCR_TRIG, LOW);
  digitalWrite(SCR_INHIB, LOW);

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

  digitalWrite(DUMP_FAN,      PowerState::DumpFan      ? HIGH : LOW);
  digitalWrite(DUMP_RELAY,    PowerState::DumpRelay    ? HIGH : LOW);
  digitalWrite(CHARGER_RELAY, PowerState::ChargerRelay ? HIGH : LOW); 
  digitalWrite(SCR_TRIG,  PowerState::ScrTrig  ? HIGH : LOW);
  digitalWrite(SCR_INHIB, PowerState::ScrInhib ? HIGH : LOW);



}

int get_output_enable_state() {
  return PowerState::outputEnabled ? 1 : 0;
} 

int dump_fan(int state) {
  digitalWrite(DUMP_FAN, (state != 0) ? HIGH : LOW);
  return 1;
}

int dump_relay(int state) {
  digitalWrite(DUMP_RELAY, (state != 0) ? HIGH : LOW);
  return 1;
}

int charger_relay(int state) {
  digitalWrite(CHARGER_RELAY, (state != 0) ? HIGH : LOW);
  return 1;
} 

int scr_trig(int state) {
    digitalWrite(SCR_TRIG, (state != 0) ? HIGH : LOW);
    return PowerState::ScrTrig ? 1 : 0;
}
int scr_inhib(int state) {
    digitalWrite(SCR_INHIB, (state != 0) ? HIGH : LOW);
    return 1;
}

