#include "Current.h"
#include "PowerState.h"

// Choose what calibrated *current* equals 100% duty on PWM (actual current -> MEASURED_CURR_OUT).
static constexpr float CURRENT_PWM_FULL_SCALE = 360.0f; // example: 100 A

// Choose what *set current* equals 100% duty on PWM (setpoint -> DPIN_IGBT_HS).
static constexpr float SET_CURRENT_PWM_FULL_SCALE = 360.0f; // example: 100 A

static AnalogReadFunc currentReader = nullptr;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

void init_current() {
  // Analog input and PWM outputs
  pinMode(APIN_CURRENT_PROBE, INPUT);
  pinMode(MEASURED_CURR_OUT, OUTPUT);  // measured current -> PWM
  pinMode(DPIN_IGBT_HS, OUTPUT);       // set current     -> PWM   (NEW)
}

void update_current() {
  // ----- Read & calibrate actual current -----
  int raw_adc = currentReader ? currentReader(APIN_CURRENT_PROBE)
                              : analogRead(APIN_CURRENT_PROBE);
  float voltage_in = (raw_adc / 4095.0f) * 3.3f;

  // Existing calibration (unchanged)
  float calculatedCurrent = (voltage_in - 1.65f) * (1.75f / 1.65f);
  calculatedCurrent = calculatedCurrent / (VScale_C);
  PowerState::probeCurrent = -(calculatedCurrent + VOffset_C);

  // ----- Map ACTUAL current to MEASURED_CURR_OUT (0–255 duty) -----
  float curr_norm = PowerState::probeCurrent / CURRENT_PWM_FULL_SCALE;
  if (curr_norm < 0.0f) curr_norm = 0.0f;
  if (curr_norm > 1.0f) curr_norm = 1.0f;
  int curr_duty8 = (int)(curr_norm * 255.0f + 0.5f);
  analogWrite(MEASURED_CURR_OUT, curr_duty8);

  // ----- Map SET current to DPIN_IGBT_HS (0–255 duty)  (NEW) -----
  float set_norm = PowerState::setCurrent / SET_CURRENT_PWM_FULL_SCALE;
  if (set_norm < 0.0f) set_norm = 0.0f;
  if (set_norm > 1.0f) set_norm = 1.0f;
  int set_duty8 = (int)(set_norm * 255.0f + 0.5f);
  analogWrite(DPIN_IGBT_HS, set_duty8);
}
