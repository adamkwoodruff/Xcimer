#include "Current.h"
#include "PowerState.h"
#include "Config.h"

static AnalogReadFunc currentReader = nullptr;
static float filteredCurrent = 0.0f;
static const float ALPHA = 0.1f;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

void init_current() {
  pinMode(APIN_COIL_CURRENT, INPUT);
}

void update_current() {
  int raw_adc = currentReader ? currentReader(APIN_COIL_CURRENT)
                              : analogRead(APIN_COIL_CURRENT);
  float voltage_in = (raw_adc / 4095.0f) * 3.3f;
  float measured = VScale_C * voltage_in + VOffset_C;

  filteredCurrent += ALPHA * (measured - filteredCurrent);
  PowerState::probeCurrent = filteredCurrent;

  float duty = filteredCurrent / I_FULL_SCALE;
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;
  PowerState::measCurrentPwmDuty = duty;
}

float get_curr_act() { return PowerState::probeCurrent; }


