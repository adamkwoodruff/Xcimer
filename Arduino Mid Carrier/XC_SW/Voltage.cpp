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
  float voltage_in = (raw_adc / 4095.0f) * 3.3f ;
  //float calculatedVoltage = VScale_V * voltage_in + VOffset_V; 

  float calculatedVoltage = (voltage_in - 1.65f) * VScale_V + VOffset_V;
  // The actual center is 1.65v - that is our zero.
  // 0 volts in corresponds to -1000v input. 3.3v corresponds to 1000v input. 
  // This may need to be negated, do to an idiot choice in PCB design, but not sure.
  //calculatedVoltage + VOffset_V;
  calculatedVoltage = (calculatedVoltage / 1000.0f) * 200.0f; 
  // UI has kV unit, so a conversion is necessary, else the system will send V and UI will represent that as kV
  // 1000v in to 0-3.3 -> -1000-1000 but UI says k
  // Divide by 1000v to bring V to kV
  // Then multiply by 200 since there are 200 CW stages in the entire system

  PowerState::probeVoltageOutput = calculatedVoltage;

// Debug output
//char buf[128];
//snprintf(buf, sizeof(buf), "[Voltage] ADC=%d | Vin=%.4f V | Calculated=%.4f", raw_adc, voltage_in, calculatedVoltage);
//Serial.println(buf);
}
