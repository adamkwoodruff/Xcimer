#include "IGBT.h"

// Helper: clamp with 5%/95% deadbands on normalized [0..1]
static inline float clamp_with_deadbands_0to1(float x) {
  if (x <= 0.05f) return 0.0f;
  if (x >= 0.95f) return 1.0f;
  return x;
}

bool igbt_fault_active() {
  // Active-low fault feedback; prefer pull-up for safety
  return digitalRead(DPIN_GATE_FAULT) == LOW;
}

void init_igbt() {
  pinMode(DPIN_IGBT_HS, OUTPUT);
  pinMode(DPIN_GATE_FAULT, INPUT_PULLUP);

  // Try to set PWM to 500 Hz on the HS pin
  #if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040)
    analogWriteFrequency(DPIN_IGBT_HS, (uint32_t)IGBT_PWM_FREQ_HZ); // 500 Hz from Config.cpp
  #endif

  // Use default analogWrite resolution
  analogWrite(DPIN_IGBT_HS, 0); // start safe
}

void update_igbt() {
  // Sample fault once and reflect it in PowerState
  const bool fault = igbt_fault_active();
  PowerState::IgbtFaultState = fault;

  if (fault) {
    // Inhibit drive if a gate fault is active
    analogWrite(DPIN_IGBT_HS, 0);
    return;
  }

  // ---- Normal (fault-free) path below ----

  // Derive desired duty (%) from setCurrent.
  // If <=100, treat as a direct percent; otherwise scale by CURRENT_LIMIT_MAX.
  float duty_pct = PowerState::setCurrent;
  if (duty_pct > 100.0f) {
    duty_pct = (PowerState::setCurrent / CURRENT_LIMIT_MAX) * 100.0f;
  }
  if (duty_pct < 0.0f)  duty_pct = 0.0f;
  if (duty_pct > 100.0f) duty_pct = 100.0f;

  // Apply clamp rule: ≤5% → 0%, ≥95% → 100%, else passthrough
  auto clamp_with_deadbands_0to1 = [](float x) {
    if (x <= 0.05f) return 0.0f;
    if (x >= 0.95f) return 1.0f;
    return x;
  };

  float duty_norm = clamp_with_deadbands_0to1(duty_pct / 100.0f);
  int duty8 = (int)(duty_norm * 255.0f + 0.5f);
  analogWrite(DPIN_IGBT_HS, duty8);
}

