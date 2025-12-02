#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include <stdbool.h>

// Battery monitor configuration
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_7  // GPIO35
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12
#define BATTERY_ADC_WIDTH       ADC_BITWIDTH_12

// Voltage divider: R1=3.3kΩ, R2=1kΩ
#define VOLTAGE_DIVIDER_RATIO   4.3

// Li-ion voltage thresholds
#define BATTERY_VOLTAGE_MAX     4.2f
#define BATTERY_VOLTAGE_MIN     3.0f
#define BATTERY_VOLTAGE_NOMINAL 3.7f

// Function prototypes - ALL NOW TAKE adc_handle AS PARAMETER
esp_err_t battery_monitor_init(adc_oneshot_unit_handle_t adc_handle);
float battery_get_voltage(adc_oneshot_unit_handle_t adc_handle);
uint8_t battery_get_percentage(adc_oneshot_unit_handle_t adc_handle);
bool battery_is_charging(adc_oneshot_unit_handle_t adc_handle);
const char* battery_get_status_string(adc_oneshot_unit_handle_t adc_handle);

#endif

