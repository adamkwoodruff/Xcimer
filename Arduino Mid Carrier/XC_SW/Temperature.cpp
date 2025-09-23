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
  float volt = (raw_adc / 4095.0f) * 3.3f;
  float temperature = -7.22f
                    + 121.0f * volt
                    - 69.3f * volt * volt
                    + 18.4f * volt * volt * volt;

  PowerState::internalTemperature = temperature;
}

float get_internal_temperature() {
  return PowerState::internalTemperature;
}
