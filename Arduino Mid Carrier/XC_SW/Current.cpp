#include "Current.h"
#include "PowerState.h"
#include "Config.h"
#include "IIRFilter.h"
#include "IGBT.h"
#include <Arduino.h>
#include "stm32h7xx_hal.h"

// Shared PWM controls provided in Voltage.cpp
extern bool ensure_measured_pwm_timer();
extern bool measured_pwm_ready();
extern void measured_pwm_set_duty(uint32_t channel, float duty_norm);


static AnalogReadFunc currentReader = nullptr;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

static BiquadIIR current_adc_filter(
    0.06745527f, 0.13491055f, 0.06745527f,
   -1.1429805f,  0.4128016f);

void init_current() {
  pinMode(APIN_CURRENT_PROBE, INPUT);
  pinMode(MEASURED_CURR_OUT, OUTPUT);

  ensure_measured_pwm_timer();
}

void update_current() {
  float calcCurr = PowerState::probeCurrent;

  if (igbt_drive_is_low()) {
    int raw_adc = currentReader ? currentReader(APIN_CURRENT_PROBE)
                                : analogRead(APIN_CURRENT_PROBE);

    float filtered_adc = current_adc_filter.process(static_cast<float>(raw_adc));

    float vin = (filtered_adc / 4095.0f) * 3.3f;

    calcCurr = (vin - 1.65f) * VScale_C + VOffset_C;
    PowerState::probeCurrent = calcCurr;
  }

  constexpr float SCR_FIRE_A = 4000.0f;
  const bool fire = (calcCurr > SCR_FIRE_A);
  PowerState::ScrTrig  = fire;
  PowerState::ScrInhib = !fire;

  // --- Map probe current to PWM duty ---
  float duty_norm = (calcCurr + 4250.0f) / 8500.0f;
  if (duty_norm < 0.0f)   duty_norm = 0.0f;
  if (duty_norm > 1.0f)   duty_norm = 1.0f;

  if (!measured_pwm_ready()) {
    if (!ensure_measured_pwm_timer()) {
      return;
    }
  }

  measured_pwm_set_duty(TIM_CHANNEL_3, duty_norm);
}

