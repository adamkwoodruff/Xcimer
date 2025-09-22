#include "IGBT.h"
#include <math.h>  // optional

static inline int duty8_from_norm(float x) {
  if (x <= 0.0f) return 0;
  if (x >= 1.0f) return 255;
  return (int)(x * 255.0f + 0.5f);
}

bool igbt_fault_active() {
  // Active-low fault feedback; prefer pull-up for safety
  return digitalRead(DPIN_GATE_FAULT) == LOW;
}

void init_igbt() {
  pinMode(DPIN_IGBT_HS, OUTPUT);
  pinMode(DPIN_GATE_FAULT, INPUT_PULLUP);

#if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040)
  analogWriteFrequency(DPIN_IGBT_HS, (uint32_t)IGBT_PWM_FREQ_HZ); // 500 Hz
#endif
  analogWrite(DPIN_IGBT_HS, 0); // start safe
}

void update_igbt() {
  // Latch and publish the gate-driver fault
  const bool fault = igbt_fault_active();
  PowerState::IgbtFaultState = fault;

  // Hard inhibit on fault, not enabled, or over-voltage
  if (fault || !PowerState::outputEnabled || (PowerState::probeVoltageOutput >= OVER_VOLTAGE_LIMIT)) {
    analogWrite(DPIN_IGBT_HS, 0);
    return;
  }

  // "Running" definition: either a waveform is armed/running or a nonzero set current exists
  const bool running = (PowerState::runCurrentWave || (PowerState::setCurrent > 0.0f));
  if (!running) {
    analogWrite(DPIN_IGBT_HS, 0);
    return;
  }

  // --- Predict maximum deliverable current at 100% duty ---
  float v_bank = PowerState::probeVoltageOutput;
  if (v_bank < 0.0f) v_bank = 0.0f;

  const float R_min = (MIN_LOAD_RES_OHM > 0.0f) ? MIN_LOAD_RES_OHM : 1e9f; // guard divide-by-zero
  const float I_pred_max = v_bank / R_min;   // Amps (same units as set/probe currents)

  // If we have no headroom, don't drive
  if (I_pred_max <= 0.0f) {
    analogWrite(DPIN_IGBT_HS, 0);
    return;
  }

  // Clamp the requested set current to controller limits
  float I_set = PowerState::setCurrent;
  if (I_set < 0.0f)               I_set = 0.0f;
  if (I_set > CURRENT_LIMIT_MAX)  I_set = CURRENT_LIMIT_MAX;

  // Upper duty limit from available headroom
  float duty_upper = I_set / I_pred_max; // 0..1 ideally
  if (duty_upper < 0.0f) duty_upper = 0.0f;
  if (duty_upper > 1.0f) duty_upper = 1.0f;

  // If measured current already exceeds the setpoint, stop pushing
  const bool above_target = (PowerState::probeCurrent > I_set);
  float duty_preset = above_target ? 0.0f : duty_upper;  // 0..1

  // Apply your min/max safety windows
  const float preset_pct = duty_preset * 100.0f;

  if (preset_pct < IGBT_MIN_DUTY_PCT) {
    // SAFETY: avoid too-short ON pulses
    analogWrite(DPIN_IGBT_HS, 0);
    return;
  }

  if (preset_pct > IGBT_MAX_DUTY_PCT) {
    // SAFETY: Force 100%
    analogWrite(DPIN_IGBT_HS, 255);
    return;
  }

  // Normal drive
  analogWrite(DPIN_IGBT_HS, duty8_from_norm(duty_preset));
}
