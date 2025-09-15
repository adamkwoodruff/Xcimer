#include <RPC.h>
#include <ArduinoJson.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SerialComms.h"
#include "Voltage.h"
#include "Current.h"
#include "EnableControl.h"
#include "Config.h"
#include "PowerState.h" 
#include "SerialRPC.h" 


void init_serial_comms() {
  //Serial.println("→ RPC.begin");
  RPC.begin();  
  //Serial.println("✓ RPC.begin");

  //Serial.println("→ Binding RPC functions...");
  RPC.bind("get_poll_data", []() -> uint64_t {
  return get_poll_data();
  });


  RPC.bind("mode_set", [](int mode){
      //RPC.call("set_value", "mode_set", mode);
      return true;
  });

  RPC.bind("process_event_in_uc", [](std::string s) {
    ////Serial.println("[RAW-RPC] " + String(s.c_str()));
    //Serial.flush();
    return process_event_in_uc(s);
  }); 

  RPC.bind("volt_act", get_volt_act);
  RPC.bind("volt_set", get_volt_set);
  RPC.bind("curr_act", get_curr_act);
  RPC.bind("curr_set", get_curr_set);
  RPC.bind("inter_enable", get_internal_enable_state);
  RPC.bind("extern_enable", get_external_enable_state);
  RPC.bind("warn_lamp", get_warn_lamp_test_state); 
  RPC.bind("dump_fan", get_dump_fan_state); 
  RPC.bind("dump_relay", get_dump_relay_state); 
  RPC.bind("charger_relay", get_charger_relay_state); 
  RPC.bind("scr_trig", get_scr_trig_state);
  RPC.bind("scr_inhib", get_scr_inhib_state); 
  RPC.bind("igbt_fault", get_igbt_fault_state);
  ////Serial.println("✓ RPC functions bound.");
}

float get_volt_set() { return PowerState::setVoltage; }
float get_curr_set() { return PowerState::setCurrent; }
int get_warn_lamp_test_state() { return PowerState::warnLampTestState ? 1 : 0; }
float get_volt_act() { return PowerState::probeVoltageOutput; }
float get_curr_act() { return PowerState::probeCurrent; }

int get_internal_enable_state() { return PowerState::internalEnable ? 1 : 0; }
int get_external_enable_state() { return PowerState::externalEnable ? 1 : 0; } 

int get_dump_fan_state() { return PowerState::DumpFan ? 1 : 0; }
int get_dump_relay_state() { return PowerState::DumpRelay ? 1 : 0; } 
int get_charger_relay_state() { return PowerState::ChargerRelay ? 1 : 0; } 
int get_scr_trig_state() { return PowerState::ScrTrig ? 1 : 0; }
int get_scr_inhib_state() { return PowerState::ScrInhib ? 1 : 0; } 
int get_igbt_fault_state() { return PowerState::IgbtFaultState ? 1 : 0; }


int process_event_in_uc(const std::string& json_event_std)
{
    StaticJsonDocument<300> doc;
    if (deserializeJson(doc, json_event_std)) {
        return 0; // parse error
    }

    JsonObject ev = doc.containsKey("display_event")
        ? doc["display_event"].as<JsonObject>()
        : doc.as<JsonObject>();
    if (ev.isNull()) return 0;

    const char* name = ev["name"] | "";
    float value = 0.0f;
    JsonVariant jv = ev["value"];
    if      (jv.is<float>())        value = jv.as<float>();
    else if (jv.is<int>())          value = jv.as<int>();
    else if (jv.is<const char*>())  value = atof(jv.as<const char*>());

    if      (strcmp(name, "volt_set") == 0) { if (value < 0) value = 0; PowerState::setVoltage     = value; return 1; }
    else if (strcmp(name, "curr_set") == 0) { if (value < 0) value = 0; PowerState::setCurrent     = value; return 1; }
    else if (strcmp(name, "inter_enable") == 0) { PowerState::internalEnable = (value != 0.0f); return 1; }
    else if (strcmp(name, "extern_enable") == 0) { PowerState::externalEnable = (value != 0.0f); return 1; }
    else if (strcmp(name, "warn_lamp") == 0) { PowerState::warnLampTestState = (value != 0.0f); return 1; } 
    else if (strcmp(name, "dump_fan") == 0) { PowerState::DumpFan = (value != 0.0f); return 1; } 
    else if (strcmp(name, "dump_relay") == 0) { PowerState::DumpRelay = (value != 0.0f); return 1; } 
    else if (strcmp(name, "charger_relay") == 0) { PowerState::ChargerRelay = (value != 0.0f); return 1; } 
    else if (strcmp(name, "scr_trig") == 0) { PowerState::ScrTrig = (value != 0.0f); return 1; } 
    else if (strcmp(name, "scr_inhib") == 0) { PowerState::ScrInhib = (value != 0.0f); return 1; }
    else if (strcmp(name, "run_current_wave") == 0) { PowerState::runCurrentWave = (value != 0.0f); return 1; }
    else if (strcmp(name, "t1") == 0)  { PowerState::currT1    = value; return 1; }
    else if (strcmp(name, "t2") == 0)  { PowerState::currT2    = value; return 1; }
    else if (strcmp(name, "th") == 0)  { PowerState::currTHold = value; return 1; }
    else if (strcmp(name, "a1") == 0)  { PowerState::currA1    = value; return 1; }
    else if (strcmp(name, "b1") == 0)  { PowerState::currB1    = value; return 1; }
    else if (strcmp(name, "c1") == 0)  { PowerState::currC1    = value; return 1; }
    else if (strcmp(name, "d1") == 0)  { PowerState::currD1    = value; return 1; }
    else if (strcmp(name, "a2") == 0)  { PowerState::currA2    = value; return 1; }
    else if (strcmp(name, "b2") == 0)  { PowerState::currB2    = value; return 1; }
    else if (strcmp(name, "c2") == 0)  { PowerState::currC2    = value; return 1; }
    else if (strcmp(name, "d2") == 0)  { PowerState::currD2    = value; return 1; }




    return 0; // unknown name
}


static inline uint32_t clamp19(uint32_t x) { return (x > 409599U) ? 409599U : x; }

uint64_t get_poll_data() {
  uint32_t v100   = clamp19((uint32_t)lroundf(PowerState::probeVoltageOutput * 100.0f));
  uint32_t c100   = clamp19((uint32_t)lroundf(PowerState::probeCurrent       * 100.0f));
  uint32_t e      = PowerState::externalEnable ? 1U : 0U;
  uint32_t igbt_f = PowerState::IgbtFaultState ? 1U : 0U;
  uint32_t ScrT   = PowerState::ScrTrig ? 1U : 0U;
  uint32_t ScrIn  = PowerState::ScrInhib ? 1U : 0U;
  uint32_t s100   = clamp19((uint32_t)lroundf(PowerState::setCurrent         * 100.0f)); 
  uint32_t RCuWa  = PowerState::runCurrentWave ? 1U : 0U;

  uint64_t word =  (uint64_t)v100
                 | ((uint64_t)c100 << 19)
                 | ((uint64_t)e    << 38)
                 | ((uint64_t)igbt_f << 39)
                 | ((uint64_t)ScrT << 40)
                 | ((uint64_t)ScrIn << 41)
                 | ((uint64_t)s100 << 42) 
                 | ((uint64_t)RCuWa << 61);

  return word;
}
