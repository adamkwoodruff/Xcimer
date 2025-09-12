#ifndef SERIALCOMMS_H
#define SERIALCOMMS_H

#include <Arduino.h>
#include <string>
#include "Config.h"
#include "PowerState.h"
#include "SerialRPC.h"

// ---------- Sync status codes (moved from .ino so .cpp can see them) ----------
#ifndef M4_STATUS_UNKNOWN
#define M4_STATUS_UNKNOWN         0x0000
#define M4_STATUS_NOT_SYNCED      0xA0B0
#define M4_STATUS_SYNCHRONISING   0xA0B1
#define M4_STATUS_SYNCED          0xA0B2
#define M4_STATUS_ERROR           0xA0FF
#endif

// These are defined in your .ino; referenced from .cpp
extern volatile bool     m4_sync_done;
extern volatile uint16_t m4_status;

// Initializes RPC and binds functions
void init_serial_comms();

// --- RPC-accessed getters (exposed to Linux) ---
float get_volt_set();
float get_curr_set();
int   get_warn_lamp_test_state();
int   get_dump_relay_state();
int   get_dump_fan_state();
int   get_charger_relay_state(); 
int get_scr_trig_state();
int get_scr_inhib_state(); 
int get_igbt_fault_state();
float get_volt_act();
float get_curr_act();
float get_analog_reading();
uint64_t get_poll_data();

// --- Sync / truth table RPCs ---
uint16_t get_sync_status_rpc();                       // returns M4_STATUS_* code
int      has_sync_completed_rpc();                    // returns 0/1
int      set_truth_table_rpc(const std::string& js);  // returns 0/1

// --- Enable state getters ---
int get_internal_enable_state();
int get_external_enable_state();

// --- Unified JSON processor (returns int ack) ---
int process_event_in_uc(const std::string& json_event);

#endif // SERIALCOMMS_H