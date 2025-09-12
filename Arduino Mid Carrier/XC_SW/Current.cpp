#include "Current.h"
#include "PowerState.h"
#include "Config.h"

// Helper: clamp with 5%/95% deadbands on normalized [0..1]
static inline float clamp_with_deadbands_0to1(float x) {
  if (x <= 0.05f) return 0.0f;
  if (x >= 0.95f) return 1.0f;
  return x;
}

static AnalogReadFunc currentReader = nullptr;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

void init_current() {
  pinMode(APIN_CURRENT_PROBE, INPUT);
  pinMode(MEASURED_CURR_OUT, OUTPUT);

  // Target 10 kHz PWM on MEASURED_CURR_OUT, if supported on this core
  #if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040)
    analogWriteFrequency(MEASURED_CURR_OUT, 10000);
  #endif
  // Keep default resolution used by analogWrite()
}

void update_current() {
  // ----- Read & calibrate actual current (unchanged base chain) -----
  int raw_adc   = currentReader ? currentReader(APIN_CURRENT_PROBE)
                                : analogRead(APIN_CURRENT_PROBE);
  float vin     = (raw_adc / 4095.0f) * 3.3f;
  float calcCur = (vin - 1.65f) * (1.75f / 1.65f);
  calcCur       = calcCur / (VScale_C);
  PowerState::probeCurrent = -(calcCur + VOffset_C);

  constexpr float SCR_FIRE_A = 3200.0f;
  const bool fire = (PowerState::probeCurrent > SCR_FIRE_A);

  // Internal booleans (always opposite)
  PowerState::ScrTrig  = fire;      // true -> pin LOW (fire)
  PowerState::ScrInhib = !fire;     // false -> pin HIGH (fire)


  // ----- Map probe current −4250…+4250 A → 0…3000 A -----
  // Linear map: -4250 → 0 ; +4250 → 3000
  float scaled0to3000 = (PowerState::probeCurrent + 4250.0f) * (3000.0f / 8500.0f);
  if (scaled0to3000 < 0.0f)   scaled0to3000 = 0.0f;
  if (scaled0to3000 > 3000.0f) scaled0to3000 = 3000.0f;

  // Normalize to 0..1 and apply deadbands (5%/95%)
  float duty_norm = scaled0to3000 / 3000.0f;
  duty_norm = clamp_with_deadbands_0to1(duty_norm);

  int duty8 = (int)(duty_norm * 255.0f + 0.5f);
  analogWrite(MEASURED_CURR_OUT, duty8);

  // NOTE: IGBT HI PWM is now handled in IGBT.cpp (not here).
}
