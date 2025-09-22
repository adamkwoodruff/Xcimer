#include "Voltage.h"
#include "PowerState.h"
#include "Config.h" 
#include "SerialRPC.h"

// 100% duty reference for measured voltage display
static constexpr float VOLTAGE_PWM_FULL_SCALE = 400.0f; //NEEDS UPDATING

static AnalogReadFunc voltageReader = nullptr;

void set_voltage_analog_reader(AnalogReadFunc func) {
  voltageReader = func;
}

void init_voltage() {
  pinMode(APIN_VOLTAGE_PROBE, INPUT);
  pinMode(MEASURED_VOLT_OUT, OUTPUT);

  // Target 10 kHz PWM on MEASURED_VOLT_OUT, if supported
  #if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040)
    analogWriteFrequency(MEASURED_VOLT_OUT, 10000);
  #endif
}

void update_voltage() { 
  // --- Read and convert ADC value ---
  int   raw_adc     = voltageReader ? voltageReader(APIN_VOLTAGE_PROBE)
                                    : analogRead(APIN_VOLTAGE_PROBE);
  float vin         = (raw_adc / 4095.0f) * 3.3f;

  // --- Apply calibration and scaling ---
  float calcVolt    = -(vin - 1.65f) * VScale_V + VOffset_V;

  PowerState::probeVoltageOutput = calcVolt;



  // --- Normalize and generate PWM output ---
  float norm = calcVolt / VOLTAGE_PWM_FULL_SCALE;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;

  int duty8 = (int)(norm * 255.0f + 0.5f);
  analogWrite(MEASURED_VOLT_OUT, duty8);
}