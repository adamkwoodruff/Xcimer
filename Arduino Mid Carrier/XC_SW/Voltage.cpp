#include "Voltage.h"
#include "PowerState.h"
#include "Config.h"

static AnalogReadFunc voltageReader = nullptr;
static float filteredVoltage = 0.0f;
static const float ALPHA = 0.1f; // IIR coefficient

void set_voltage_analog_reader(AnalogReadFunc func) {
  voltageReader = func;
}

void init_voltage() {
  pinMode(APIN_CAP_VOLTAGE, INPUT);
}

void update_voltage() {
  int raw_adc = voltageReader ? voltageReader(APIN_CAP_VOLTAGE)
                              : analogRead(APIN_CAP_VOLTAGE);
  float voltage_in = (raw_adc / 4095.0f) * 3.3f;
  float measured = VScale_V * voltage_in + VOffset_V;

  filteredVoltage += ALPHA * (measured - filteredVoltage);
  PowerState::probeVoltageOutput = filteredVoltage;

  // scale for PWM-DAC
  float duty = filteredVoltage / V_FULL_SCALE;
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;
  PowerState::measVoltagePwmDuty = duty;
}

float get_volt_act() { return PowerState::probeVoltageOutput; }


