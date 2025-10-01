#include "Current.h"
#include "PowerState.h"
#include "Config.h"
#include "IGBT.h"
#include "stm32h7xx_hal.h"

// ---------- HAL TIM1 (shared timer) state ----------
static TIM_HandleTypeDef s_tim1 = {};
static bool              s_tim1_inited = false;

// Map normalized duty [0..1] to CCR given current ARR
static inline uint32_t duty_to_ccr(float duty_norm) {
  if (duty_norm <= 0.0f) return 0U;
  float dn = (duty_norm >= 1.0f) ? 1.0f : duty_norm;
  const uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&s_tim1);
  uint32_t ccr = (uint32_t)(dn * (float)(arr + 1U) + 0.5f);
  if (ccr > arr) ccr = arr;
  return ccr;
}

// Ensure TIM1 is configured for 10 kHz center-aligned PWM on CH2/CH3.
// We (re)configure both channels so either module can set up safely.
static void ensure_tim1_10khz_pwm() {
  if (s_tim1_inited) return;

  // --- Clocks & GPIO ---
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();

  // PA9  -> TIM1_CH2 (AF1)
  // PA10 -> TIM1_CH3 (AF1)
  GPIO_InitTypeDef gpio = {};
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Pin       = GPIO_PIN_9;
  gpio.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin       = GPIO_PIN_10;
  HAL_GPIO_Init(GPIOA, &gpio);

  // --- Compute PSC/ARR for 10 kHz center-aligned (TIM clk ≈ 200 MHz like TIM3) ---
  const uint32_t timer_clock_hz = 200000000U;   // Portenta H7 typical TIM1 clock
  const float    f_pwm          = 10000.0f;     // 10 kHz
  uint32_t total_period_ticks   = (uint32_t)( (double)timer_clock_hz / (2.0 * f_pwm) ); // center-aligned

  uint32_t psc = 0, arr = 0;
  while (1) {
    arr = (total_period_ticks / (psc + 1U)) - 1U;
    if (arr <= 65535U) break;
    if (++psc > 65535U) return; // impossible config
  }

  // --- Init TIM1 ---
  s_tim1.Instance               = TIM1;
  s_tim1.Init.Prescaler         = psc;
  s_tim1.Init.CounterMode       = TIM_COUNTERMODE_CENTERALIGNED1;
  s_tim1.Init.Period            = arr;
  s_tim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  s_tim1.Init.RepetitionCounter = 0;
  s_tim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&s_tim1) != HAL_OK) return;

  // Common OC settings
  TIM_OC_InitTypeDef oc = {};
  oc.OCMode       = TIM_OCMODE_PWM1;
  oc.Pulse        = 0U;
  oc.OCPolarity   = TIM_OCPOLARITY_HIGH;     
  oc.OCFastMode   = TIM_OCFAST_DISABLE;
  oc.OCIdleState  = TIM_OCIDLESTATE_RESET;

  // Configure CH2 (PA9) and CH3 (PA10)
  if (HAL_TIM_PWM_ConfigChannel(&s_tim1, &oc, TIM_CHANNEL_2) != HAL_OK) return;
  if (HAL_TIM_PWM_ConfigChannel(&s_tim1, &oc, TIM_CHANNEL_3) != HAL_OK) return;

  // Start both channels (safe to leave duty at 0 until we set compare)
  if (HAL_TIM_PWM_Start(&s_tim1, TIM_CHANNEL_2) != HAL_OK) return;
  if (HAL_TIM_PWM_Start(&s_tim1, TIM_CHANNEL_3) != HAL_OK) return;

  // Apply registers
  __HAL_TIM_SET_PRESCALER(&s_tim1, psc);
  __HAL_TIM_SET_AUTORELOAD(&s_tim1, arr);
  __HAL_TIM_SET_COMPARE(&s_tim1, TIM_CHANNEL_2, 0U);
  __HAL_TIM_SET_COMPARE(&s_tim1, TIM_CHANNEL_3, 0U);

  s_tim1_inited = true;
}

static AnalogReadFunc currentReader = nullptr;
void set_current_analog_reader(AnalogReadFunc func) { currentReader = func; }

static float filtered_probe_current = 0.0f;
static bool  current_filter_initialized = false;

void init_current() {
  pinMode(APIN_CURRENT_PROBE, INPUT);
  pinMode(MEASURED_CURR_OUT, OUTPUT);
  ensure_tim1_10khz_pwm(); // set up TIM1 hardware PWM @10kHz
}

void update_current() {
  // --- MEASUREMENT GATE ---
  // Only measure current internally when a waveform is running on the IGBT PWM.
  // Otherwise, report 0.0 A and force the display PWM to 0%.
  if (!PowerState::runCurrentWave) {
    PowerState::probeCurrent = 0.0f;
    // Keep SCR outputs in a safe default state when not running
    PowerState::ScrTrig  = false; // HIGH on pin (no fire)
    PowerState::ScrInhib = true;  // LOW on pin (inhibit active)

    if (s_tim1_inited) {
      __HAL_TIM_SET_COMPARE(&s_tim1, TIM_CHANNEL_3, 0U); // current display PWM = 0%
    }
    // Do not integrate/filter ADC while idle to avoid stale drift
    filtered_probe_current = 0.0f;
    current_filter_initialized = false;
    return;
  }

  if (igbt_drive_is_low()) {
    // --- Read ADC value and apply simple IIR filtering ---
    const int raw_adc = currentReader ? currentReader(APIN_CURRENT_PROBE)
                                      : analogRead(APIN_CURRENT_PROBE);

    const float vin = ((float)raw_adc / 4095.0f) * 3.3f;
    const float sample_current = (vin - 1.65f) * VScale_C + VOffset_C;

    if (!current_filter_initialized) {
      filtered_probe_current = sample_current;
      current_filter_initialized = true;
    } else {
      filtered_probe_current = (0.9f * filtered_probe_current) + (0.1f * sample_current);
    }
  }

  PowerState::probeCurrent = current_filter_initialized ? filtered_probe_current : 0.0f;

  // SCR logic
  constexpr float SCR_FIRE_A = 4000.0f;
  const bool fire = (PowerState::probeCurrent > SCR_FIRE_A);
  PowerState::ScrTrig  = fire;
  PowerState::ScrInhib = !fire;

  if (!s_tim1_inited) return;

  // --- Duty mapping: -4250 → 0%, +4250 → 100% ---
  float duty_norm = (PowerState::probeCurrent + 4250.0f) / 8500.0f;
  if (duty_norm < 0.0f) duty_norm = 0.0f;
  if (duty_norm > 1.0f) duty_norm = 1.0f;

  // Drive TIM1_CH3 (PA10) for current display PWM
  __HAL_TIM_SET_COMPARE(&s_tim1, TIM_CHANNEL_3, duty_to_ccr(duty_norm));
}
