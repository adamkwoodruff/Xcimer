#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include "Config.h"
#include "PowerState.h"

typedef int (*AnalogReadFunc)(uint8_t);
void set_temperature_analog_reader(AnalogReadFunc func);

void init_temperature();
void update_temperature();
float get_internal_temperature();

#endif // TEMPERATURE_H
