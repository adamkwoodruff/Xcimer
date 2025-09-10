#include "Voltage.h"
#include "PowerState.h"

static AnalogReadFunc voltageReader = nullptr;

void set_voltage_analog_reader(AnalogReadFunc func) {
  voltageReader = func;
}

void init_voltage() {
  // Set pin mode for analog input
  pinMode(APIN_VOLTAGE_PROBE, INPUT);
}

void update_voltage() {
  int raw_adc = voltageReader ? voltageReader(APIN_VOLTAGE_PROBE)
                              : analogRead(APIN_VOLTAGE_PROBE);
  float voltage_in = (raw_adc / 4095.0f) * 3.3f;
  float calculatedVoltage = VScale_V * voltage_in + VOffset_V; 

  PowerState::probeVoltageOutput = calculatedVoltage;

  // Debug output
//char buf[128];
//snprintf(buf, sizeof(buf), "[Voltage] ADC=%d | Vin=%.4f V | Calculated=%.4f", raw_adc, voltage_in, calculatedVoltage);
//Serial.println(buf);


}
