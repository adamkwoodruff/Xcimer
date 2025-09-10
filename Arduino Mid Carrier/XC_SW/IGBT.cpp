// IGBT.cpp - Corrected TIM3(M)/TIM1(S) Synchronization

#include "IGBT.h"
#include "Config.h"
#include "PowerState.h"
#include <Arduino.h>
#include "stm32h7xx_hal.h" 


//extern volatile float current_volt_set;

// handles for TIM3 (master) and TIM1 (slave)
static TIM_HandleTypeDef htim3;
static TIM_HandleTypeDef htim1;

void init_igbt_pwm() {
  HAL_StatusTypeDef status; 
  //Serial.println("[IGBT] Initializing TIM3(M)/TIM1(S) Synchronized PWM...");

  // 1) GPIO & clock enables
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();
  //Serial.println("[IGBT] Clocks Enabled.");

  // 2) Configure GPIOs
  GPIO_InitTypeDef gpio = {};
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL; 
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  gpio.Pin       = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOC, &gpio);
  //Serial.println("[IGBT] GPIOC Pin 7 Configured (TIM3_CH2).");

  //    PA9 = TIM1_CH2 (AF1)
  gpio.Alternate = GPIO_AF1_TIM1;
  gpio.Pin       = GPIO_PIN_9;
  HAL_GPIO_Init(GPIOA, &gpio);
  //Serial.println("[IGBT] GPIOA Pin 9 Configured (TIM1_CH2).");

  // --- Calculate Common Period (ARR) ---
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq(); 
  Serial.print("[IGBT] PCLK1 (TIM3 Source Freq): "); //Serial.println(pclk1);
  Serial.print("[IGBT] PCLK2 (TIM1 Source Freq): "); //Serial.println(pclk2);

  if (pclk1 == 0 || IGBT_PWM_FREQ_HZ <= 0) {
      //Serial.println("[IGBT] Error: PCLK1 clock or PWM frequency is zero or invalid!");
      return; 
  }
  
  uint32_t common_period_arr = static_cast<uint32_t>((static_cast<float>(pclk1) / (IGBT_PWM_FREQ_HZ * 2.0f)) - 1.0f);
  Serial.print("[IGBT] Common ARR Calculated (from PCLK1): "); //Serial.println(common_period_arr);

   if (common_period_arr < 10) { 
       //Serial.println("[IGBT] Warning: Calculated Timer Period (ARR) is very small!");
   }
  // --- End Common Period Calculation ---


  // 3) Configure TIM3 master (center-aligned)
  {
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_CENTERALIGNED1; 
    Serial.print("[IGBT] Common ARR Calculated (from PCLK1): ");
    //Serial.println(common_period_arr);
    htim3.Init.Period            = common_period_arr; 
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.RepetitionCounter = 0; 
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; 

    status = HAL_TIM_PWM_Init(&htim3);
    if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM3 HAL_TIM_PWM_Init failed! Status: "); //Serial.println(status); 
    return; }

   
    TIM_MasterConfigTypeDef mcfg = {};
    mcfg.MasterOutputTrigger = TIM_TRGO_UPDATE; 
    mcfg.MasterSlaveMode     = TIM_MASTERSLAVEMODE_ENABLE; 
    status = HAL_TIMEx_MasterConfigSynchronization(&htim3, &mcfg);
    if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM3 HAL_TIMEx_MasterConfigSynchronization failed! Status: "); //Serial.println(status); 
    return; }
    //Serial.println("[IGBT] TIM3 Master Configured (TRGO on Update).");
  }

  // 4) Configure TIM1 slave (center-aligned)
  {
    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0; 
  
    htim1.Init.CounterMode       = TIM_COUNTERMODE_CENTERALIGNED1;
    htim1.Init.Period            = common_period_arr; 
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0; 
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; 

    status = HAL_TIM_PWM_Init(&htim1);
    if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM1 HAL_TIM_PWM_Init failed! Status: "); //Serial.println(status); 
    return; }

   
    TIM_BreakDeadTimeConfigTypeDef bdtr = {};
    bdtr.OffStateRunMode  = TIM_OSSR_DISABLE;
    bdtr.OffStateIDLEMode = TIM_OSSI_DISABLE;
    bdtr.LockLevel        = TIM_LOCKLEVEL_OFF;
    bdtr.DeadTime         = 0; 
    bdtr.BreakState       = TIM_BREAK_DISABLE;
    bdtr.BreakPolarity    = TIM_BREAKPOLARITY_HIGH; 
    bdtr.BreakFilter      = 0;
    bdtr.AutomaticOutput  = TIM_AUTOMATICOUTPUT_ENABLE; 
    status = HAL_TIMEx_ConfigBreakDeadTime(&htim1, &bdtr);
    if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM1 HAL_TIMEx_ConfigBreakDeadTime failed! Status: "); //Serial.println(status); 
    return; }
    //Serial.println("[IGBT] TIM1 BDTR Configured (MOE Enabled).");

    // Configure slave mode: Trigger on ITR2 (Input Trigger from TIM3 TRGO)
    TIM_SlaveConfigTypeDef scfg = {};
    scfg.SlaveMode        = TIM_SLAVEMODE_TRIGGER; 
    scfg.InputTrigger     = TIM_TS_ITR2;         
    scfg.TriggerPolarity  = TIM_TRIGGERPOLARITY_NONINVERTED; 
    scfg.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;     
    scfg.TriggerFilter    = 0;                           

    status = HAL_TIM_SlaveConfigSynchro(&htim1, &scfg);
    if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM1 HAL_TIM_SlaveConfigSynchro failed! Status: "); //Serial.println(status); 
    return; }
    //Serial.println("[IGBT] TIM1 Slave Configured (Trigger on ITR2 from TIM3).");
  }

  // 5) Configure PWM output channels
  TIM_OC_InitTypeDef oc = {};
  oc.OCMode       = TIM_OCMODE_PWM1; 
  oc.Pulse        = 0;              
  oc.OCPolarity   = TIM_OCPOLARITY_LOW; // this, and the Nploraity, have been changed high -> low since transistors are low side
  oc.OCNPolarity  = TIM_OCNPOLARITY_LOW; 
  oc.OCFastMode   = TIM_OCFAST_DISABLE;
  oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
  oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

  status = HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2);
  if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM3 HAL_TIM_PWM_ConfigChannel CH2 failed! Status: "); //Serial.println(status); 
  return; }
  //Serial.println("[IGBT] TIM3 CH2 (PC7) Configured. Polarity: HIGH.");

  // Configure TIM1_CH2 (PA9) - HIGH Polarity (Complement)
  oc.OCMode     = TIM_OCMODE_PWM2;     
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;  
  status = HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_2);
   if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM1 HAL_TIM_PWM_ConfigChannel CH2 failed! Status: "); //Serial.println(status); 
   return; }
  //Serial.println("[IGBT] TIM1 CH2 (PA9) Configured. Polarity: LOW (Inverted).");


  // 6) Start PWM Output Generation for both channels
  status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2); // Start Slave Channel
  if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM1 HAL_TIM_PWM_Start CH2 failed! Status: "); //Serial.println(status); 
  return; }
  //Serial.println("[IGBT] TIM1 CH2 PWM Started.");

  status = HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); // Start Master Channel
  if (status != HAL_OK) { Serial.print("[IGBT] Error: TIM3 HAL_TIM_PWM_Start CH2 failed! Status: "); //Serial.println(status); HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2); 
  return; }
  //Serial.println("[IGBT] TIM3 CH2 PWM Started.");


  //Serial.println("[IGBT] TIM3_CH2<->TIM1_CH2 center-aligned PWM initialization complete."); 

  __HAL_TIM_SET_AUTORELOAD(&htim3, common_period_arr);
  __HAL_TIM_SET_AUTORELOAD(&htim1, common_period_arr);
  Serial.print("[IGBT] Forced ARR set to: ");
  //Serial.println(__HAL_TIM_GET_AUTORELOAD(&htim3));

  pinMode(DPIN_GATE_FAULT, INPUT_PULLUP);
  pinMode(DPIN_GATE_RESET, OUTPUT);
  digitalWrite(DPIN_GATE_RESET, LOW);
  pinMode(DPIN_ENABLE_IN, INPUT_PULLUP);
}


void update_igbt_pwm() {
  static float integral = 0.0f;
  static float lastErr  = 0.0f;
  static unsigned long lastUs = 0;

  // Latch fault
  if (digitalRead(DPIN_GATE_FAULT) == LOW) {
    PowerState::faultLockout = true;
  }

  // Handle reset pulse
  if (PowerState::pwmResetActive) {
    digitalWrite(DPIN_GATE_RESET, HIGH);
    if (millis() >= PowerState::pwmResetEndMs) {
      PowerState::pwmResetActive = false;
      digitalWrite(DPIN_GATE_RESET, LOW);
      PowerState::faultLockout = false; // allow re-enable after reset
    }
  }

  bool hwEnable = digitalRead(DPIN_ENABLE_IN) == HIGH;  
  PowerState::faultLockout = false;
  bool allowOut = PowerState::pwmEnable && !PowerState::psDisable && !PowerState::faultLockout && hwEnable;
  
  float iSet = allowOut ? PowerState::setCurrent : 0.0f;
  if (PowerState::probeVoltageOutput > PowerState::setVoltage) iSet = 0.0f;
  iSet = constrain(iSet, 0.0f, CURRENT_LIMIT_MAX);

  float error = iSet - PowerState::probeCurrent;
  unsigned long nowUs = micros();
  float dt = (lastUs > 0) ? (nowUs - lastUs) / 1e6f : 0.0f;
  lastUs = nowUs;

  integral += error * dt;
  float derivative = (dt > 0) ? (error - lastErr) / dt : 0.0f;
  float pidOut = PID_KP * error + PID_KI * integral + PID_KD * derivative;
  lastErr = error;

  if (!allowOut || iSet <= 0.0f) {
    integral = 0.0f;
    pidOut = 0.0f;
  }

  float duty = pidOut * PID_TO_DUTY_SCALE;

  float dtf = (IGBT_PWM_DEAD_TIME_NS * 1e-9f) * IGBT_PWM_FREQ_HZ;
  dtf = constrain(dtf, 0.0f, IGBT_PWM_MAX_DUTY / 2.0f);
  duty = constrain(duty, IGBT_PWM_MIN_DUTY, IGBT_PWM_MAX_DUTY - dtf);
  if (!allowOut) duty = 0.0f;

  volatile uint32_t arr = htim3.Instance->ARR;
  if (arr == 0) return;
  uint32_t cmp = static_cast<uint32_t>(duty * static_cast<float>(arr + 1));
  if (cmp > arr) cmp = arr;

  bool pwmAllowed = !PowerState::psDisable && PowerState::pwmEnable;
  uint32_t outCmp = pwmAllowed ? cmp : 0; 

  char debugLine[512];
  snprintf(debugLine, sizeof(debugLine),
    "[PWM] pwmEnable:%d psDisable:%d faultLockout:%d hwEnable:%d allowOut:%d "
    "setCurrent:%.2f probeCurrent:%.2f setVoltage:%.2f probeVoltage:%.2f iSet:%.2f "
    "error:%.2f pidOut:%.2f duty:%.3f outCmp:%lu ARR:%lu FinalDuty%%:%.2f",
    PowerState::pwmEnable, PowerState::psDisable, PowerState::faultLockout, hwEnable, allowOut,
    PowerState::setCurrent, PowerState::probeCurrent,
    PowerState::setVoltage, PowerState::probeVoltageOutput, iSet,
    error, pidOut, duty, outCmp, arr, ((float)outCmp / (arr + 1)) * 100.0f
  );
  //Serial.println(debugLine);




  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, outCmp);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, outCmp); 

  
}