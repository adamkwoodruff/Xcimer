#include "Voltage.h"
#include "PowerState.h"

// Choose what calibrated *voltage* equals 100% duty on PWM.
// Adjust to your system's expected range.
static constexpr float VOLTAGE_PWM_FULL_SCALE = 200.0f; // example: 200 (same unit as probeVoltageOutput)

static AnalogReadFunc voltageReader = nullptr;

void set_voltage_analog_reader(AnalogReadFunc func) {
  voltageReader = func;
}

void init_voltage() {
  // Set pin mode for analog input & PWM output
  pinMode(APIN_VOLTAGE_PROBE, INPUT);
  pinMode(MEASURED_VOLT_OUT, OUTPUT);  // NEW: PWM out
}

void update_voltage() {
  int raw_adc = voltageReader ? voltageReader(APIN_VOLTAGE_PROBE)
                              : analogRead(APIN_VOLTAGE_PROBE);
  float voltage_in = (raw_adc / 4095.0f) * 3.3f ;

  // Existing calibration chain (unchanged)
  float calculatedVoltage = (voltage_in - 1.65f) * VScale_V + VOffset_V;
  calculatedVoltage = (calculatedVoltage / 1000.0f) * 200.0f;

  // Publish calibrated reading
  PowerState::probeVoltageOutput = calculatedVoltage;

  // --- NEW: drive PWM proportionally to the calibrated value ---
  // Map negative readings to 0% duty (clip at 0).
  float norm = calculatedVoltage / VOLTAGE_PWM_FULL_SCALE;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;

  // Arduino analogWrite() takes 0..255 on this core
  int duty8 = (int)(norm * 255.0f + 0.5f);
  analogWrite(MEASURED_VOLT_OUT, duty8);
}
