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
#define RL_VALUE            10.0            // Load resistance in kOhm

// Smoke detection thresholds (PPM)
#define SMOKE_THRESHOLD_LOW     800   // Detectable smoke
#define SMOKE_THRESHOLD_MEDIUM  1500  // Moderate smoke - warning
#define SMOKE_THRESHOLD_HIGH    2500  // Heavy smoke - fire likely

// Smoke detection status
typedef enum {
    SMOKE_CLEAR = 0,
    SMOKE_DETECTED,
    SMOKE_WARNING,
    SMOKE_FIRE_ALERT
} smoke_level_t;

// Function prototypes
esp_err_t mq135_init(void);
void mq135_deinit(void);
uint32_t mq135_get_raw_adc(void);
float mq135_get_voltage(void);
float mq135_get_ppm(void);
smoke_level_t mq135_get_smoke_level(void);
bool mq135_is_smoke_detected(void);
const char* mq135_get_status_string(smoke_level_t level);

#endif

