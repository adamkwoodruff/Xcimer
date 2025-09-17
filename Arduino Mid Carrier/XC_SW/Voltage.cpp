#include "Voltage.h"
#include "PowerState.h"
#include "Config.h"
#include "MeasuredPWM.h"

// 100% duty reference for measured voltage display
static constexpr float VOLTAGE_PWM_FULL_SCALE = 200.0f;

// Helper: clamp with 5%/95% deadbands on normalized [0..1]
static inline float clamp_with_deadbands_0to1(float x) {
  if (x <= 0.05f) return 0.0f;
  if (x >= 0.95f) return 1.0f;
  return x;
}

static AnalogReadFunc voltageReader = nullptr;

void set_voltage_analog_reader(AnalogReadFunc func) {
  voltageReader = func;
}

void init_voltage() {
  pinMode(APIN_VOLTAGE_PROBE, INPUT);
  pinMode(MEASURED_VOLT_OUT, OUTPUT);

#if defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_MBED)
  measured_pwm_init();
#elif defined(TEENSYDUINO) || defined(ARDUINO_ARCH_RP2040)
  analogWriteFrequency(MEASURED_VOLT_OUT, 10000);
#endif
}

void update_voltage() {
  int   raw_adc     = voltageReader ? voltageReader(APIN_VOLTAGE_PROBE)
                                    : analogRead(APIN_VOLTAGE_PROBE);
  float vin         = (raw_adc / 4095.0f) * 3.3f;

  // Existing calibration chain (unchanged)
  float calcVolt    = (vin - 1.65f) * VScale_V + VOffset_V;
  calcVolt          = (calcVolt / 1000.0f) * 200.0f;

  PowerState::probeVoltageOutput = calcVolt;

  // Normalize and clamp PWM duty
  float norm = calcVolt / VOLTAGE_PWM_FULL_SCALE;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;
  norm = clamp_with_deadbands_0to1(norm);

#if defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_MBED)
  measured_pwm_set_voltage_norm(norm);
#else
  int duty8 = (int)(norm * 255.0f + 0.5f);
  analogWrite(MEASURED_VOLT_OUT, duty8);
#endif
}
