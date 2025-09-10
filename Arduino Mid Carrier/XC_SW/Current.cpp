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
  //float calculatedCurrent = VScale_C * voltage_in + VOffset_C; 
  float calculatedCurrent = VScale_C * voltage_in + VOffset_C;

  PowerState::probeCurrent = calculatedCurrent; 
  //PowerState::probeCurrent = PowerState::setCurrent * 0.5;  // Fake feedback at 50%


  // Debug output
//char buf[128];
//snprintf(buf, sizeof(buf), "[Current] ADC=%d | Vin=%.4f V | Calculated=%.4f", raw_adc, voltage_in, calculatedCurrent);
//Serial.println(buf);

}
