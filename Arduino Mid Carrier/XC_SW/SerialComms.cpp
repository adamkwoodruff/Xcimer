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

  ////Serial.println("✓ RPC functions bound.");
}

float get_volt_set() { return PowerState::setVoltage; }
float get_curr_set() { return PowerState::setCurrent; }
int get_warn_lamp_test_state() { return PowerState::warnLampTestState ? 1 : 0; }
float get_volt_act() { return PowerState::probeVoltageOutput; }
float get_curr_act() { return PowerState::probeCurrent; }

int get_internal_enable_state() { return PowerState::internalEnable ? 1 : 0; }
int get_external_enable_state() { return PowerState::externalEnable ? 1 : 0; }

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


    return 0; // unknown name
}


static inline uint32_t clamp19(uint32_t x) { return (x > 409599U) ? 409599U : x; }

uint64_t get_poll_data() {
  uint32_t v100 = clamp19((uint32_t)lroundf(PowerState::probeVoltageOutput * 100.0f));
  uint32_t c100 = clamp19((uint32_t)lroundf(PowerState::probeCurrent       * 100.0f));
  uint32_t e    = PowerState::externalEnable ? 1U : 0U;

  uint64_t word =  (uint64_t)v100
                 | ((uint64_t)c100 << 19)
                 | ((uint64_t)e    << 38);


  return word;
}
