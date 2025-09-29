#include "Voltage.h"
#include "PowerState.h"
#include "Config.h" 
#include "SerialRPC.h"

static AnalogReadFunc voltageReader = nullptr; 


const float VOLTAGE_ADC_FILTER_ALPHA = 0.1f;

static float voltage_filtered_raw_adc = -1.0f;

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
  // --- Read ADC value ---
  int raw_adc = voltageReader ? voltageReader(APIN_VOLTAGE_PROBE)
                              : analogRead(APIN_VOLTAGE_PROBE);

  // --- NEW: Apply Exponential Moving Average (EMA) Filter ---
  if (voltage_filtered_raw_adc < 0.0f) {
    // On the first run, initialize the filter with the first reading.
    voltage_filtered_raw_adc = (float)raw_adc;
  } else {
    // Apply the filter to smooth out the reading.
    voltage_filtered_raw_adc = (VOLTAGE_ADC_FILTER_ALPHA * (float)raw_adc) + (1.0f - VOLTAGE_ADC_FILTER_ALPHA) * voltage_filtered_raw_adc;
  }

  // --- Convert filtered ADC value to voltage ---
  // CORRECTED: Use 4095.0f for the Portenta's 12-bit ADC
  float vin = (lroundf(voltage_filtered_raw_adc) / 4095.0f) * 3.3f;

  // --- Apply calibration and scaling (now on a stable value) ---
  float calcVolt = (vin - 1.65f) * VScale_V + VOffset_V;
  PowerState::probeVoltageOutput = calcVolt;

  // --- Normalize and generate PWM output ---
  float scale = VOLTAGE_PWM_FULL_SCALE;
  if (scale <= 0.0f) {
    scale = 1.0f;
  }

  float norm = calcVolt / scale;
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;

  int duty8 = (int)(norm * 255.0f + 0.5f);
  analogWrite(MEASURED_VOLT_OUT, duty8);
}