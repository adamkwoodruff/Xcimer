#include "Current.h"
#include "PowerState.h"
#include "Config.h"


static AnalogReadFunc currentReader = nullptr;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

// NEW: Smoothing factor for the ADC filter.
const float CURRENT_ADC_FILTER_ALPHA = 0.1f;

static float current_filtered_raw_adc = -1.0f;

void init_current() {
  pinMode(APIN_CURRENT_PROBE, INPUT);
  pinMode(MEASURED_CURR_OUT, OUTPUT);

  // Target 10 kHz PWM on MEASURED_VOLT_OUT, if supported
  #if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040)
    analogWriteFrequency(MEASURED_CURR_OUT, 10000);
  #endif
}

void update_current() {
  // --- Read ADC value ---
  int raw_adc = currentReader ? currentReader(APIN_CURRENT_PROBE)
                              : analogRead(APIN_CURRENT_PROBE);

  // --- NEW: Apply Exponential Moving Average (EMA) Filter ---
  if (current_filtered_raw_adc < 0.0f) {
    current_filtered_raw_adc = (float)raw_adc;
  } else {
    current_filtered_raw_adc = (CURRENT_ADC_FILTER_ALPHA * (float)raw_adc) + (1.0f - CURRENT_ADC_FILTER_ALPHA) * current_filtered_raw_adc;
  }

  // --- Convert filtered ADC value to voltage ---
  // CORRECTED: Use 4095.0f for the Portenta's 12-bit ADC
  float vin = (lroundf(current_filtered_raw_adc) / 4095.0f) * 3.3f; 

  // --- Apply calibration and scaling ---
  float calcCurr = (vin - 1.65f) * VScale_C + VOffset_C;
  PowerState::probeCurrent = calcCurr;

  constexpr float SCR_FIRE_A = 3200.0f;
  const bool fire = (PowerState::probeCurrent > SCR_FIRE_A);
  PowerState::ScrTrig  = fire;
  PowerState::ScrInhib = !fire;

  // --- Map probe current to PWM duty ---
  float duty_norm = (PowerState::probeCurrent + 4250.0f) / 8500.0f;
  if (duty_norm < 0.0f)   duty_norm = 0.0f;
  if (duty_norm > 1.0f)   duty_norm = 1.0f;

  int duty8 = (int)(duty_norm * 255.0f + 0.5f);
  analogWrite(MEASURED_CURR_OUT, duty8);
}