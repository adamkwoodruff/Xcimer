#include "Current.h"
#include "PowerState.h"

static AnalogReadFunc currentReader = nullptr;

void set_current_analog_reader(AnalogReadFunc func) {
  currentReader = func;
}

void init_current() {
  // Set pin mode for analog input
  pinMode(APIN_CURRENT_PROBE, INPUT);
}

void update_current() {
  int raw_adc = currentReader ? currentReader(APIN_CURRENT_PROBE)
                              : analogRead(APIN_CURRENT_PROBE);
  float voltage_in = (raw_adc / 4095.0f) * 3.3f;
  // could be seeing -1.75 to 1.75 volts input

  float calculatedCurrent = (voltage_in - 1.65f) * (1.75f / 1.65f); 
  calculatedCurrent = calculatedCurrent / (VScale_C); // where VScale_C is the resistor
  PowerState::probeCurrent = -(calculatedCurrent + VOffset_C); 


  // Debug output
//char buf[128];
//snprintf(buf, sizeof(buf), "[Current] ADC=%d | Vin=%.4f V | Calculated=%.4f", raw_adc, voltage_in, calculatedCurrent);
//Serial.println(buf);

}
