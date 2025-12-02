#ifndef MQ135_DRIVER_H
#define MQ135_DRIVER_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <stdbool.h>

// MQ135 Configuration
#define MQ135_ADC_UNIT      ADC_UNIT_1
#define MQ135_ADC_CHANNEL   ADC_CHANNEL_6  // GPIO34
#define MQ135_ADC_ATTEN     ADC_ATTEN_DB_12
#define MQ135_ADC_WIDTH     ADC_BITWIDTH_12

// MQ135 sensor constants
#define MQ135_RL            10.0f   // Load resistance in kOhm
#define MQ135_R0            10.0f   // Sensor resistance in clean air (calibrated value)
#define MQ135_VC            5.0f    // Circuit voltage (5V typically)

// Curve fitting constants for smoke/CO2 (adjust based on datasheet)
#define MQ135_A             116.6020682f
#define MQ135_B             -2.769034857f

// Smoke detection thresholds (PPM)
#define MQ135_DETECTION_THRESHOLD   800   // Detectable smoke
#define MQ135_WARNING_THRESHOLD     1500  // Moderate smoke - warning
#define MQ135_FIRE_THRESHOLD        2500  // Heavy smoke - fire likely

// Smoke detection status
typedef enum {
    SMOKE_CLEAR = 0,
    SMOKE_DETECTED,
    SMOKE_WARNING,
    SMOKE_FIRE_ALERT
} smoke_level_t;

// Function prototypes - ALL NOW TAKE adc_handle AS PARAMETER
esp_err_t mq135_init(adc_oneshot_unit_handle_t adc_handle);
void mq135_deinit(void);  // No ADC handle needed for cleanup
uint32_t mq135_get_raw_adc(adc_oneshot_unit_handle_t adc_handle);
float mq135_get_voltage(adc_oneshot_unit_handle_t adc_handle);
float mq135_get_ppm(adc_oneshot_unit_handle_t adc_handle);
smoke_level_t mq135_get_smoke_level(adc_oneshot_unit_handle_t adc_handle);
bool mq135_is_smoke_detected(adc_oneshot_unit_handle_t adc_handle);
const char* mq135_get_status_string(smoke_level_t level);  // No ADC handle needed (just string conversion)

#endif

