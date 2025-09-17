#pragma once

#include <Arduino.h>

#ifdef ARDUINO_ARCH_STM32
// Initializes the shared timer driving the measured voltage/current outputs.
void measured_pwm_init();
// Update the PWM duty for the voltage feedback (normalized 0.0..1.0).
void measured_pwm_set_voltage_norm(float duty_norm);
// Update the PWM duty for the current feedback (normalized 0.0..1.0).
void measured_pwm_set_current_norm(float duty_norm);
#else
inline void measured_pwm_init() {}
inline void measured_pwm_set_voltage_norm(float) {}
inline void measured_pwm_set_current_norm(float) {}
#endif

