#include "IGBT.h"
#include "Config.h"
#include "PowerState.h"
#include <Arduino.h>
#include "stm32h7xx_hal.h"
#include <math.h>

// ----- TIM3 (PC7 / CH2) state -----
static TIM_HandleTypeDef s_tim3 = {};
static bool              s_pwm_started = false; 

// --- Helpers --------------------------------------------------------------

static inline float clamp01(float x) {
  if (x <= 0.0f) return 0.0f;
  if (x >= 1.0f) return 1.0f;
  return x;
}

static inline float clamp_with_deadbands_0to1(float x) {
  if (x <= 0.05f) return 0.0f;
  if (x >= 0.95f) return 1.0f;
  return x;
}

static inline uint32_t duty_to_ccr(float duty_norm) {
  if (duty_norm <= 0.0f) return 0U;
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&s_tim3);
  uint32_t ccr = (uint32_t)(duty_norm * (float)(arr + 1U) + 0.5f);
  if (ccr > arr) ccr = arr;
  return ccr;
}

static inline void pwm_off() {
  if (s_pwm_started) __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_2, 0U);
}

static inline void pwm_full_on() {
  if (!s_pwm_started) return;
  const uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&s_tim3);
  __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_2, arr);
}

bool igbt_fault_active() {
  return (digitalRead(DPIN_GATE_FAULT) == LOW);
}

void init_igbt() {
  // Fault input
  pinMode(DPIN_GATE_FAULT, INPUT_PULLUP);

  // Clocks & GPIO for TIM3_CH2 on PC7 (AF2)
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {};
  gpio.Pin       = GPIO_PIN_7;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOC, &gpio);

  // --- NEW: DYNAMIC FREQUENCY CALCULATION ---
  if (IGBT_PWM_FREQ_HZ <= 0.0f) {
    s_pwm_started = false;
    return; // Cannot configure with zero or negative frequency
  }

  const uint32_t timer_clock_hz = 200000000; // Known TIM3 clock on Portenta
  // For center-aligned mode, the total period ticks = clock / (2 * frequency)
  uint32_t total_period_ticks = (uint32_t)(timer_clock_hz / (2.0f * IGBT_PWM_FREQ_HZ));

  uint32_t psc = 0;
  uint32_t arr = 0;

  // TIM3 is a 16-bit timer, so ARR and PSC must be <= 65535.
  // This loop finds the smallest prescaler (psc) that allows the
  // auto-reload register (arr) to fit within the 16-bit limit.
  while (true) {
    arr = (total_period_ticks / (psc + 1)) - 1;
    if (arr <= 65535) {
      break; // Found a valid combination
    }
    psc++;
    // If psc is also too large, the requested frequency is too low to be possible.
    if (psc > 65535) {
      s_pwm_started = false;
      return;
    }
  }
  // --- END OF DYNAMIC CALCULATION ---

  s_tim3.Instance               = TIM3;
  s_tim3.Init.Prescaler         = psc; // Use calculated prescaler
  s_tim3.Init.CounterMode       = TIM_COUNTERMODE_CENTERALIGNED1;
  s_tim3.Init.Period            = arr; // Use calculated period
  s_tim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  s_tim3.Init.RepetitionCounter = 0;
  s_tim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&s_tim3) != HAL_OK) { s_pwm_started = false; return; }

  TIM_OC_InitTypeDef oc = {};
  oc.OCMode       = TIM_OCMODE_PWM1;
  oc.Pulse        = 0U;
  oc.OCPolarity   = TIM_OCPOLARITY_LOW;
  oc.OCFastMode   = TIM_OCFAST_DISABLE;
  oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&s_tim3, &oc, TIM_CHANNEL_2) != HAL_OK) { s_pwm_started = false; return; }

  if (HAL_TIM_PWM_Start(&s_tim3, TIM_CHANNEL_2) != HAL_OK) { s_pwm_started = false; return; }

  __HAL_TIM_SET_PRESCALER(&s_tim3, psc);
  __HAL_TIM_SET_AUTORELOAD(&s_tim3, arr);
  __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_2, 0U);

  s_pwm_started = true;
}

// --- UPDATED FOR TESTING ---
void update_igbt() {
  if (!s_pwm_started) return;

  // Latch and publish the gate-driver fault
  const bool fault = igbt_fault_active();
  PowerState::IgbtFaultState = fault;

  // Hard inhibits: fault, not enabled, or over-voltage
  if (fault || !PowerState::outputEnabled ||
      (PowerState::probeVoltageOutput >= OVER_VOLTAGE_LIMIT)) {
    pwm_off();
    return;
  }

  // “Running” if waveform is armed/running OR a non-zero set current exists
  const bool running = (PowerState::runCurrentWave || (PowerState::setCurrent > 0.0f));
  if (!running) {
    pwm_off();
    return;
  }

  // Predict maximum deliverable current at 100% duty
  float v_bank = PowerState::probeVoltageOutput;
  if (v_bank < 0.0f) v_bank = 0.0f;

  const float R_min = (MIN_LOAD_RES_OHM > 0.0f) ? MIN_LOAD_RES_OHM : 1e9f; // guard divide-by-zero
  const float I_pred_max = v_bank / R_min;   // Amps

  // No headroom → don’t drive
  if (I_pred_max <= 0.0f) {
    pwm_off();
    return;
  }

  // Clamp requested current to limits
  float I_set = PowerState::setCurrent;
  if (I_set < 0.0f)              I_set = 0.0f;
  if (I_set > CURRENT_LIMIT_MAX) I_set = CURRENT_LIMIT_MAX;

  // Upper duty limit from available headroom (0..1)
  float duty_upper = I_set / I_pred_max;
  duty_upper = clamp01(duty_upper);

  // If measured current already exceeds the setpoint, stop pushing
  const bool above_target = (PowerState::probeCurrent > I_set);
  float duty_preset = above_target ? 0.0f : duty_upper;   // normalized 0..1

  // Safety windows
  const float preset_pct = duty_preset * 100.0f;

  if (preset_pct < IGBT_MIN_DUTY_PCT) {
    // Avoid too-short ON pulses near zero
    pwm_off();
    return;
  }

  if (preset_pct > IGBT_MAX_DUTY_PCT) {
    // Force 100% (still respects center-aligned PWM)
    pwm_full_on();
    return;
  }

  // Normal drive (optionally apply soft deadbands)
  float duty_norm = clamp_with_deadbands_0to1(duty_preset);
  __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_2, duty_to_ccr(duty_norm)); 
 
}