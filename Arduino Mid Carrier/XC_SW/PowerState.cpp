#include "PowerState.h"

// Initialise static members
volatile float PowerState::setVoltage        = 0.0f;
volatile float PowerState::setCurrent        = 0.0f;
volatile float PowerState::probeVoltageOutput = 0.0f;
volatile float PowerState::probeCurrent      = 0.0f;

volatile bool  PowerState::igbtFault         = false;
volatile bool  PowerState::extEnable         = false;

volatile bool  PowerState::chargerRelay      = false;
volatile bool  PowerState::dumpRelay         = false;
volatile bool  PowerState::dumpFan           = false;
volatile bool  PowerState::warnLamp          = false;
volatile bool  PowerState::scrTrig_cmd       = false;
volatile bool  PowerState::scrInhibit_allow  = false;

volatile float PowerState::measVoltagePwmDuty = 0.0f;
volatile float PowerState::measCurrentPwmDuty = 0.0f;

