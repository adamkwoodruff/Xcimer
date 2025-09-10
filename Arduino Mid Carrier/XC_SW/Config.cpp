#include "Config.h"

// --- Pin definitions with polarity metadata ---
const PinDef IGBT_FAULT_IN      {PE_10, false};
const PinDef EXT_ENABLE_IN      {PF_4,  true};

const PinDef CHARGER_RELAY_CTRL {PF_6,  false};  // GPIO4
const PinDef DUMP_RELAY_CTRL    {PE_11, false};  // GPIO6
const PinDef DUMP_FAN_RELAY_CTRL{PF_2,  true};   // GPIO2
const PinDef SCR_TRIG           {PF_3,  true};   // GPIO1
const PinDef SCR_INHIBIT        {PB_10, true};   // PWM3 used as DO
const PinDef WARN_LAMP          {PF_8,  true};   // GPIO0

// --- Calibration constants ---
const float VScale_V = 1000.0f;   // volts per volt
const float VOffset_V = 0.0f;
const float VScale_C = 1000.0f;   // amps per volt
const float VOffset_C = 0.0f;

// --- Timing/threshold constants ---
const unsigned long DEBOUNCE_DELAY_US      = 1000;
const unsigned long WARN_BLINK_INTERVAL_MS = 500;  // ms
const float WARN_VOLTAGE_THRESHOLD         = 50.0f;

// --- Control parameters ---
const float CURRENT_LIMIT_MAX  = 360.0f;   // mA
const float OVER_VOLTAGE_LIMIT = 100.0f;   // kV
const float PID_KP             = 0.4f;
const float PID_KI             = 0.05f;
const float PID_KD             = 0.01f;
const float PID_TO_DUTY_SCALE  = 0.00125f;
const float IGBT_PWM_MIN_DUTY  = 0.05f;   // 5%

// --- IGBT PWM configuration ---
const float   IGBT_PWM_FREQ_HZ          = 20000.0f;
const float   IGBT_PWM_MAX_DUTY         = 0.45f;
const uint8_t IGBT_PWM_RESOLUTION_BITS  = 12;

// --- Analog via PWM scaling ---
const float V_FULL_SCALE = 1000.0f; // volts corresponding to 100% duty
const float I_FULL_SCALE = 100.0f;  // amps corresponding to 100% duty

