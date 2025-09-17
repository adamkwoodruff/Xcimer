#include "MeasuredPWM.h"

#ifdef ARDUINO_ARCH_STM32

#include "Config.h"

extern "C" {
#include "stm32h7xx_hal.h"
}

static TIM_HandleTypeDef s_measuredTim = {};
static uint32_t s_timerPeriod = 0;
static bool s_initialized = false;

static uint32_t get_tim1_clock_hz() {
  RCC_ClkInitTypeDef clk_config = {};
  uint32_t flash_latency = 0;
  HAL_RCC_GetClockConfig(&clk_config, &flash_latency);

  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
  if (clk_config.APB2CLKDivider != RCC_HCLK_DIV1) {
    pclk2 *= 2U;
  }
  return pclk2;
}

void measured_pwm_init() {
  if (s_initialized) {
    return;
  }
  s_initialized = true;

  __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {};
  GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  constexpr uint32_t kTargetFreqHz = 10000U;
  constexpr uint32_t kDesiredSteps = 1024U;

  uint32_t timer_clk = get_tim1_clock_hz();
  uint32_t prescaler = (timer_clk + (kTargetFreqHz * kDesiredSteps) - 1U) / (kTargetFreqHz * kDesiredSteps);
  if (prescaler < 1U) {
    prescaler = 1U;
  }

  uint32_t period = (timer_clk / (prescaler * kTargetFreqHz));
  if (period == 0U) {
    period = 1U;
  } else {
    period -= 1U;
  }

  while (period > 0xFFFFU) {
    prescaler++;
    period = (timer_clk / (prescaler * kTargetFreqHz));
    if (period == 0U) {
      period = 1U;
      break;
    }
    period -= 1U;
  }

  s_timerPeriod = period;

  s_measuredTim.Instance = TIM1;
  s_measuredTim.Init.Prescaler = prescaler - 1U;
  s_measuredTim.Init.CounterMode = TIM_COUNTERMODE_UP;
  s_measuredTim.Init.Period = period;
  s_measuredTim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  s_measuredTim.Init.RepetitionCounter = 0;
  s_measuredTim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_PWM_Init(&s_measuredTim);

  TIM_OC_InitTypeDef sConfigOC = {};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0U;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  HAL_TIM_PWM_ConfigChannel(&s_measuredTim, &sConfigOC, TIM_CHANNEL_2);
  HAL_TIM_PWM_ConfigChannel(&s_measuredTim, &sConfigOC, TIM_CHANNEL_3);

  HAL_TIM_PWM_Start(&s_measuredTim, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&s_measuredTim, TIM_CHANNEL_3);
}

static uint32_t duty_to_compare(float duty_norm) {
  if (duty_norm <= 0.0f) {
    return 0U;
  }
  if (duty_norm >= 1.0f) {
    return s_timerPeriod;
  }
  float scaled = duty_norm * static_cast<float>(s_timerPeriod);
  uint32_t compare = static_cast<uint32_t>(scaled + 0.5f);
  if (compare > s_timerPeriod) {
    compare = s_timerPeriod;
  }
  return compare;
}

void measured_pwm_set_voltage_norm(float duty_norm) {
  if (!s_initialized) {
    measured_pwm_init();
  }
  __HAL_TIM_SET_COMPARE(&s_measuredTim, TIM_CHANNEL_2, duty_to_compare(duty_norm));
}

void measured_pwm_set_current_norm(float duty_norm) {
  if (!s_initialized) {
    measured_pwm_init();
  }
  __HAL_TIM_SET_COMPARE(&s_measuredTim, TIM_CHANNEL_3, duty_to_compare(duty_norm));
}

#endif  // ARDUINO_ARCH_STM32

