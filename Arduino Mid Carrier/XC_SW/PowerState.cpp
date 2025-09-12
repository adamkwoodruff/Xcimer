#include "PowerState.h"

volatile float PowerState::setVoltage = 0.0f;
volatile float PowerState::setCurrent = 0.0f;
volatile float PowerState::probeVoltageOutput = 0.0f;
volatile float PowerState::probeCurrent = 0.0f;

volatile bool PowerState::internalEnable = false;
volatile bool PowerState::externalEnable = false;
volatile bool PowerState::outputEnabled  = false;

volatile bool PowerState::warnLampTestState = false;
unsigned long PowerState::lastWarnBlinkTimeMs = 0;
bool PowerState::warnLampOn = false;
  

volatile bool PowerState::DumpFan       = false;
volatile bool PowerState::DumpRelay     = false;
volatile bool PowerState::ChargerRelay  = false;

volatile bool PowerState::ScrTrig   = false;
volatile bool PowerState::ScrInhib  = false;
volatile bool PowerState::IgbtFaultState = false; 