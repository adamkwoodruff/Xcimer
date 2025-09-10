#include "EnableControl.h"
#include "PowerState.h"

void init_enable_control() {
  pinMode(DPIN_ENABLE_IN, HW_INPUT_PIN_MODE);    // External enable input
  // Initialize control outputs to match stored state
  digitalWrite(DPIN_CTRL_PS_DISABLE, PowerState::psDisable ? LOW : HIGH); // swapped low and high
  pinMode(DPIN_CTRL_PS_DISABLE, OUTPUT);
  digitalWrite(DPIN_CTRL_PWM_ENABLE, PowerState::pwmEnable ? LOW : HIGH); // swapped low and high
  pinMode(DPIN_CTRL_PWM_ENABLE, OUTPUT);
  pinMode(DPIN_WARN_LAMP_OUT, OUTPUT);           // Warning lamp output
  digitalWrite(DPIN_WARN_LAMP_OUT, LOW);
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
    digitalWrite(DPIN_WARN_LAMP_OUT, HIGH);

  } else {
    // Neither condition active - ensure the lamp stays off
    PowerState::warnLampOn = false;
    digitalWrite(DPIN_WARN_LAMP_OUT, LOW);
  }

  digitalWrite(DPIN_CTRL_PS_DISABLE, PowerState::psDisable ? LOW : HIGH); // swapped low and high
  digitalWrite(DPIN_CTRL_PWM_ENABLE, PowerState::pwmEnable ? LOW : HIGH); // swapped low and high
}

int get_output_enable_state() {
  return PowerState::outputEnabled ? 1 : 0;
}