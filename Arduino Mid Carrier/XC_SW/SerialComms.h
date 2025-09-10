#ifndef SERIALCOMMS_H
#define SERIALCOMMS_H

#include <Arduino.h>
#include <string>

void init_serial_comms();
int process_event_in_uc(const std::string& json_event);

#endif // SERIALCOMMS_H

