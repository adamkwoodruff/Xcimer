#include <RPC.h>
#include <ArduinoJson.h>
#include <string>

#include "SerialComms.h"
#include "Voltage.h"
#include "Current.h"
#include "PowerState.h"

void init_serial_comms() {
  RPC.begin();
  RPC.bind("volt_act", get_volt_act);
  RPC.bind("curr_act", get_curr_act);
  RPC.bind("igbt_fault", [](){ return PowerState::igbtFault ? 1 : 0; });
  RPC.bind("ext_enable", [](){ return PowerState::extEnable ? 1 : 0; });
  RPC.bind("charger_relay", [](){ return PowerState::chargerRelay ? 1 : 0; });
  RPC.bind("dump_relay", [](){ return PowerState::dumpRelay ? 1 : 0; });
  RPC.bind("dump_fan", [](){ return PowerState::dumpFan ? 1 : 0; });
  RPC.bind("warn_lamp", [](){ return PowerState::warnLamp ? 1 : 0; });
  RPC.bind("meas_voltage_pwm", [](){ return PowerState::measVoltagePwmDuty; });
  RPC.bind("meas_current_pwm", [](){ return PowerState::measCurrentPwmDuty; });

  RPC.bind("process_event_in_uc", process_event_in_uc);
}

int process_event_in_uc(const std::string& json_event_std) {
  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, json_event_std)) {
    return 0;
  }
  JsonObject ev = doc["display_event"].as<JsonObject>();
  const char* name = ev["name"] | "";
  float value = ev["value"] | 0.0f;

  if      (strcmp(name, "charger_relay") == 0) { PowerState::chargerRelay = (value != 0.0f); return 1; }
  else if (strcmp(name, "dump_relay")    == 0) { PowerState::dumpRelay    = (value != 0.0f); return 1; }
  else if (strcmp(name, "dump_fan")      == 0) { PowerState::dumpFan      = (value != 0.0f); return 1; }
  else if (strcmp(name, "scr_trig")      == 0) { PowerState::scrTrig_cmd  = (value != 0.0f); return 1; }

  return 0;
}

