#include "Temperature.h"

static AnalogReadFunc temperatureReader = nullptr;

void set_temperature_analog_reader(AnalogReadFunc func) {
  temperatureReader = func;
}

void init_temperature() { 
  pinMode(APIN_INTERNAL_TEMP, INPUT);
}

void update_temperature() {
  int raw_adc = temperatureReader ? temperatureReader(APIN_INTERNAL_TEMP)
                                  : analogRead(APIN_INTERNAL_TEMP);
  float t_volt = (raw_adc / 1023.0f) * 3.3f;
  float temperature = -7.22f
                    + 121.0f * t_volt
                    - 69.3f * t_volt * t_volt
                    + 18.4f * t_volt * t_volt * t_volt;

  PowerState::internalTemperature = temperature;
}

