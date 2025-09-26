#include "Current.h"
#include "PowerState.h"
#include "Config.h"


static AnalogReadFunc currentReader = nullptr;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

void init_current() {
  pinMode(APIN_CURRENT_PROBE, INPUT);
  pinMode(MEASURED_CURR_OUT, OUTPUT);

  // Target 10 kHz PWM on MEASURED_VOLT_OUT, if supported
  #if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040)
    analogWriteFrequency(MEASURED_CURR_OUT, 10000);
  #endif
}

void update_current() {
  // ----- Read & calibrate actual current (unchanged base chain) -----
  int raw_adc   = currentReader ? currentReader(APIN_CURRENT_PROBE)
                                : analogRead(APIN_CURRENT_PROBE);
  float vin     = (raw_adc / 1023.0f) * 3.3f; 

  // --- Apply calibration and scaling ---
  float calcCurr    = (vin - 1.65f) * VScale_C + VOffset_C;
  PowerState::probeCurrent = calcCurr;

  constexpr float SCR_FIRE_A = 3200.0f;
  const bool fire = (PowerState::probeCurrent > SCR_FIRE_A);

  // Internal booleans (always opposite)
  PowerState::ScrTrig  = fire;      // true -> pin LOW (fire)
  PowerState::ScrInhib = !fire;     // false -> pin HIGH (fire)


  // ----- Map probe current −4250…+4250 A linearly to 0…100% duty -----
  // Linear map: -4250 A → 0% ; 0 A → 50% ; +4250 A → 100%
  float duty_norm = (PowerState::probeCurrent + 4250.0f) / 8500.0f;
  if (duty_norm < 0.0f)   duty_norm = 0.0f;
  if (duty_norm > 1.0f)   duty_norm = 1.0f;

    int duty8 = (int)(duty_norm * 255.0f + 0.5f);
    analogWrite(MEASURED_CURR_OUT, duty8);


}