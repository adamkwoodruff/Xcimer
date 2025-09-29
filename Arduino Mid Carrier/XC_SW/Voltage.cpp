#include "Voltage.h"
#include "PowerState.h"
#include "Config.h"
#include "SerialRPC.h"
#include <Arduino.h>
#include "stm32h7xx_hal.h"

static AnalogReadFunc voltageReader = nullptr;


const float VOLTAGE_ADC_FILTER_ALPHA = 0.1f;

static float voltage_filtered_raw_adc = -1.0f;

// -----------------------------------------------------------------------------
// Shared TIM1 PWM driver for measured voltage/current outputs (PA9 / PA10)
// -----------------------------------------------------------------------------

TIM_HandleTypeDef g_measured_pwm_tim1 = {};
bool              g_measured_pwm_started = false;

static inline uint32_t measured_pwm_duty_to_ccr(float duty_norm) {
  if (duty_norm <= 0.0f) return 0U;
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&g_measured_pwm_tim1);
  uint32_t ccr = (uint32_t)(duty_norm * (float)(arr + 1U) + 0.5f);
  if (ccr > arr) ccr = arr;
  return ccr;
}

bool ensure_measured_pwm_timer() {
  if (g_measured_pwm_started) {
    return true;
  }

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {};
  gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOA, &gpio);

  g_measured_pwm_tim1.Instance               = TIM1;
  g_measured_pwm_tim1.Init.Prescaler         = 0;
  g_measured_pwm_tim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
  g_measured_pwm_tim1.Init.Period            = 19999; // 200 MHz / (0+1) / (19999+1) = 10 kHz
  g_measured_pwm_tim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  g_measured_pwm_tim1.Init.RepetitionCounter = 0;
  g_measured_pwm_tim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&g_measured_pwm_tim1) != HAL_OK) {
    g_measured_pwm_started = false;
    return false;
  }

  TIM_OC_InitTypeDef oc = {};
  oc.OCMode      = TIM_OCMODE_PWM1;
  oc.Pulse       = 0U;
  oc.OCPolarity  = TIM_OCPOLARITY_HIGH;
  oc.OCFastMode  = TIM_OCFAST_DISABLE;
  oc.OCIdleState = TIM_OCIDLESTATE_RESET;

  if (HAL_TIM_PWM_ConfigChannel(&g_measured_pwm_tim1, &oc, TIM_CHANNEL_2) != HAL_OK) {
    return false;
  }
  if (HAL_TIM_PWM_ConfigChannel(&g_measured_pwm_tim1, &oc, TIM_CHANNEL_3) != HAL_OK) {
    return false;
  }

  if (HAL_TIM_PWM_Start(&g_measured_pwm_tim1, TIM_CHANNEL_2) != HAL_OK) {
    return false;
  }
  if (HAL_TIM_PWM_Start(&g_measured_pwm_tim1, TIM_CHANNEL_3) != HAL_OK) {
    return false;
  }

  __HAL_TIM_SET_COMPARE(&g_measured_pwm_tim1, TIM_CHANNEL_2, 0U);
  __HAL_TIM_SET_COMPARE(&g_measured_pwm_tim1, TIM_CHANNEL_3, 0U);

  g_measured_pwm_started = true;
  return true;
}

bool measured_pwm_ready() {
  return g_measured_pwm_started;
}

void measured_pwm_set_duty(uint32_t channel, float duty_norm) {
  if (!g_measured_pwm_started) {
    return;
  }

  if (duty_norm <= 0.0f) {
    __HAL_TIM_SET_COMPARE(&g_measured_pwm_tim1, channel, 0U);
    return;
  }

  if (duty_norm >= 1.0f) {
    const uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&g_measured_pwm_tim1);
    __HAL_TIM_SET_COMPARE(&g_measured_pwm_tim1, channel, arr);
    return;
  }

  __HAL_TIM_SET_COMPARE(&g_measured_pwm_tim1, channel, measured_pwm_duty_to_ccr(duty_norm));
}

void set_voltage_analog_reader(AnalogReadFunc func) {
  voltageReader = func;
}

void init_voltage() {
  pinMode(APIN_VOLTAGE_PROBE, INPUT);
  pinMode(MEASURED_VOLT_OUT, OUTPUT);

  ensure_measured_pwm_timer();
}


void update_voltage() { 
  // --- Read ADC value ---
  int raw_adc = voltageReader ? voltageReader(APIN_VOLTAGE_PROBE)
                              : analogRead(APIN_VOLTAGE_PROBE);

  // --- NEW: Apply Exponential Moving Average (EMA) Filter ---
  if (voltage_filtered_raw_adc < 0.0f) {
    // On the first run, initialize the filter with the first reading.
    voltage_filtered_raw_adc = (float)raw_adc;
  } else {
    // Apply the filter to smooth out the reading.
    voltage_filtered_raw_adc = (VOLTAGE_ADC_FILTER_ALPHA * (float)raw_adc) + (1.0f - VOLTAGE_ADC_FILTER_ALPHA) * voltage_filtered_raw_adc;
  }

  // --- Convert filtered ADC value to voltage ---
  // CORRECTED: Use 4095.0f for the Portenta's 12-bit ADC
  float vin = (lroundf(voltage_filtered_raw_adc) / 4095.0f) * 3.3f;

  // --- Apply calibration and scaling (now on a stable value) ---
  float calcVolt = (vin - 1.65f) * VScale_V + VOffset_V;
  PowerState::probeVoltageOutput = calcVolt;

  // --- Normalize and generate PWM output ---
  float duty_norm = (calcVolt + 1000.0f) / 2000.0f;
  if (duty_norm < 0.0f) duty_norm = 0.0f;
  if (duty_norm > 1.0f) duty_norm = 1.0f;

  if (!measured_pwm_ready()) {
    if (!ensure_measured_pwm_timer()) {
      return;
    }
  }

  measured_pwm_set_duty(TIM_CHANNEL_2, duty_norm);
}

