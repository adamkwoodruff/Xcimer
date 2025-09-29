#include <Arduino.h>
#include "Config.h"
#include "Voltage.h"
#include "Current.h"
#include "Temperature.h"
#include "EnableControl.h"
#include "SerialComms.h"
#include "IGBT.h"
#include "CurrWaveform.h"
#include <ArduinoJson.h>
#include <RPC.h>
#include <string> 
#include "SerialRPC.h"  

// --- Sync State Codes ---
#define M4_STATUS_UNKNOWN         0x0000
#define M4_STATUS_NOT_SYNCED      0xA0B0
#define M4_STATUS_SYNCHRONISING   0xA0B1
#define M4_STATUS_SYNCED          0xA0B2
#define M4_STATUS_ERROR           0xA0FF

volatile bool m4_sync_done = false;
volatile uint16_t m4_status = M4_STATUS_NOT_SYNCED; 
bool m4_sync_status_logged = false; 



void setup() {
  Serial.begin(115200);
  //Serial.println("Portenta M4 Core Logic Starting...");
  //Serial.println("--------------------------------");
  //Serial.println("Initializing Modules...");

  init_serial_comms();
  //Serial.println("Serial OK"); 

  init_voltage();
  //Serial.println("Voltage OK");

  init_current();
  //Serial.println("Current OK");

  init_temperature();
  //Serial.println("Temperature OK");

  init_enable_control();  
  //Serial.println("Enable Control OK");

  // --- RPC Setup ---
  RPC.bind("get_sync_status", []() -> uint16_t {
    uint16_t status = m4_sync_done ? M4_STATUS_SYNCED : M4_STATUS_NOT_SYNCED;
   
    //Serial.println(status, HEX);
    return status;
  });

  RPC.bind("set_truth_table", [](const std::string& jsonString) {
    if (m4_sync_done) return;

    //Serial.println("0xA0B0 - Ready – NOT synchronized");

    m4_status = M4_STATUS_SYNCHRONISING;
    //Serial.println("0xA0B1 - Synchronising");

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, jsonString);
    if (err) {

      //Serial.println(err.c_str());
      m4_status = M4_STATUS_ERROR;
      return;
    }

    // Apply JSON values
    if (doc.containsKey("volt_set")) PowerState::setVoltage = doc["volt_set"];
    if (doc.containsKey("curr_set")) PowerState::setCurrent = doc["curr_set"];

    m4_status = M4_STATUS_SYNCED;
    m4_sync_done = true; 
    //Serial.println("0xA0B2 - Ready Synchronised");
  });

  RPC.bind("has_sync_completed", []() -> bool {
    return m4_sync_done; 
  });

  //Serial.println("RPC bindings OK");

  init_igbt();
  //Serial.println("PWM OK"); 

  //Serial.println("--------------------------------");
  //Serial.println("Setup Complete. Entering main loop.");
} 

void loop() {
  static uint32_t last_us = micros();
  uint32_t now_us = micros();
  float dt = (now_us - last_us) * 1e-6f;
  last_us = now_us;

  update_enable_inputs();
  update_voltage();
  update_current();
  update_temperature();
  
  update_curr_waveform(dt);
  update_igbt(); 

  update_enable_outputs(); 
  //delayMicroseconds(5);

  // Sync status output
  static uint32_t last_log_ms = 0;
  if (!m4_sync_done) {
    if (millis() - last_log_ms > 1000) { // Log once per second
      //Serial.println("0xA0B0 - Ready – NOT synchronized");
      last_log_ms = millis();
    }
  } else if (!m4_sync_status_logged) {
    //Serial.println("0xA0B2 - Ready – SYNCHRONIZED");
    m4_sync_status_logged = true;
  }
}