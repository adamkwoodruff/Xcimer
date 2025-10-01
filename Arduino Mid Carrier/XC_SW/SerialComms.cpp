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
#include "Temperature.h"
#include "Config.h"
#include "PowerState.h" 
#include "SerialRPC.h" 
#include "IGBT.h"
 

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
  RPC.bind("internal_temperature", get_internal_temperature);
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
float get_internal_temperature() { return PowerState::internalTemperature; }

int get_internal_enable_state() { return PowerState::internalEnable ? 1 : 0; }
int get_external_enable_state() { return PowerState::externalEnable ? 1 : 0; } 

int get_dump_fan_state() { return PowerState::DumpFan ? 1 : 0; }
int get_dump_relay_state() {
  // UI expects logical ON/OFF opposite to the active-low hardware drive
  return PowerState::DumpRelay ? 0 : 1;
}
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

    if (strcmp(name, "curr_set") == 0) {if (value < 0.0f) value = 0.0f; if (value > 3600.0f) value = 3600.0f; PowerState::setCurrent = value; return 1;}
    else if (strcmp(name, "volt_set") == 0) {if (value < 0.0f) value = 0.0f; if (value > 285.0f) value = 285.0f; PowerState::setVoltage = value; return 1;}
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
    else if (strcmp(name, "curr_scale") == 0) {
      VScale_C = value;
      return 1;
    }
    else if (strcmp(name, "curr_offset") == 0) {
      VOffset_C = value;
      return 1;
    }
    else if (strcmp(name, "volt_scale") == 0) {
      VScale_V = value;
      return 1;
    }
    else if (strcmp(name, "volt_offset") == 0) {
      VOffset_V = value;
      return 1;
    }
    else if (strcmp(name, "volt_pwm_full_scale") == 0) {
      if (value < 1.0f) value = 1.0f;
      VOLTAGE_PWM_FULL_SCALE = value;
      return 1;
    }
    else if (strcmp(name, "min_load_res_ohm") == 0) {
      if (value <= 0.0f) value = 1e-6f;
      MIN_LOAD_RES_OHM = value;
      return 1;
    }
    else if (strcmp(name, "igbt_min_duty_pct") == 0) {
      if (value < 0.0f) value = 0.0f;
      if (value > 100.0f) value = 100.0f;
      IGBT_MIN_DUTY_PCT = value;
      return 1;
    }
    else if (strcmp(name, "igbt_max_duty_pct") == 0) {
      if (value < 0.0f) value = 0.0f;
      if (value > 100.0f) value = 100.0f;
      IGBT_MAX_DUTY_PCT = value;
      return 1;
    }
    else if (strcmp(name, "current_limit_max") == 0) {
      if (value < 0.0f) value = 0.0f;
      CURRENT_LIMIT_MAX = value;
      return 1;
    }
    else if (strcmp(name, "over_voltage_limit") == 0) {
      if (value < 0.0f) value = 0.0f;
      OVER_VOLTAGE_LIMIT = value;
      return 1;
    }
    else if (strcmp(name, "warn_voltage_threshold") == 0) {
      if (value < 0.0f) value = 0.0f;
      WARN_VOLTAGE_THRESHOLD = value;
      return 1;
    }
    else if (strcmp(name, "warn_blink_interval_ms") == 0) {
      if (value < 0.0f) value = 0.0f;
      WARN_BLINK_INTERVAL_MS = (unsigned long)(value + 0.5f);
      return 1;
    }
    else if (strcmp(name, "debounce_delay_us") == 0) {
      if (value < 0.0f) value = 0.0f;
      DEBOUNCE_DELAY_US = (unsigned long)(value + 0.5f);
      return 1;
    } 
    else if (strcmp(name, "igbt_pwm_freq_hz") == 0) {
      if (value <= 0.0f) value = 1.0f; // Prevent zero/negative frequency
      IGBT_PWM_FREQ_HZ = value;
      init_igbt(); 
      return 1;
    }


    return 0; // unknown name
}


static inline int32_t clamp_s(int32_t x, int32_t lo, int32_t hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}
static inline uint64_t mask_nbits(int32_t x, unsigned n) {
  // x is already clamped to the signed n-bit range; mask to 2's complement n bits
  return (uint64_t)( (uint32_t)x & ((n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u)) );
}

// In SerialComms.cpp

uint64_t get_poll_data() {
    // Static variable to track which packet to send next
    static bool send_packet_zero = true;

    uint64_t word = 0;

    // For 20-bit signed resolution: -(2^19) to (2^19 - 1)
    const int32_t max_val = (1 << 19) - 1;
    const int32_t min_val = -(1 << 19);

    if (send_packet_zero) {
        // --- PACKET 0: ID=0, Flags, Actuals ---
        uint64_t flags =
            ((uint64_t)(PowerState::externalEnable ? 1U : 0U) << 0) |
            ((uint64_t)(PowerState::IgbtFaultState ? 1U : 0U) << 1) |
            ((uint64_t)(PowerState::ScrTrig        ? 1U : 0U) << 2) |
            ((uint64_t)(PowerState::ScrInhib       ? 1U : 0U) << 3) |
            ((uint64_t)(PowerState::runCurrentWave ? 1U : 0U) << 4);

        int32_t v100 = clamp_s((int32_t)lroundf(PowerState::probeVoltageOutput * 100.0f), min_val, max_val);
        int32_t c100 = clamp_s((int32_t)lroundf(PowerState::probeCurrent       * 100.0f), min_val, max_val);

        word |= (0ULL)                     << 63;  // Packet ID = 0
        word |= (flags & 0x1FULL)          << 58;  // [62:58] Flags (5 bits)
        word |= (mask_nbits(v100, 20))     << 38;  // [57:38] volt_act (20 bits)
        word |= (mask_nbits(c100, 20))     << 18;  // [37:18] curr_act (20 bits)

    } else {
        // --- PACKET 1: ID=1, Setpoints, Temperature ---
        int32_t s100 = clamp_s((int32_t)lroundf(PowerState::setCurrent          * 100.0f), min_val, max_val);
        int32_t t100 = clamp_s((int32_t)lroundf(PowerState::internalTemperature * 100.0f), min_val, max_val);

        word |= (1ULL)                     << 63;  // Packet ID = 1
        word |= (mask_nbits(s100, 20))     << 43;  // [62:43] curr_set (20 bits)
        word |= (mask_nbits(t100, 20))     << 23;  // [42:23] internal_temp (20 bits)
    }

    // Toggle for the next call
    send_packet_zero = !send_packet_zero;

    return word;
}