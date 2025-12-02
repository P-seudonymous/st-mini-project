#include "mq135_driver.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "MQ135";
// REMOVED: static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

// Smoke curve parameters
#define PARA_A  605.18   
#define PARA_B  -3.937  

esp_err_t mq135_init(adc_oneshot_unit_handle_t adc_handle) {
    esp_err_t ret;
    
    // REMOVED: adc_oneshot_new_unit code - ADC1 already created in main.c
    
    // Configure ADC channel only
    adc_oneshot_chan_cfg_t config = {
        .atten = MQ135_ADC_ATTEN,
        .bitwidth = MQ135_ADC_WIDTH,
    };
    ret = adc_oneshot_config_channel(adc_handle, MQ135_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        return ret;
    }
    
    // Initialize ADC calibration
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = MQ135_ADC_UNIT,
        .atten = MQ135_ADC_ATTEN,
        .bitwidth = MQ135_ADC_WIDTH,
    };
    
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ ADC calibration: Line Fitting Scheme");
    } else {
        ESP_LOGW(TAG, "⚠ ADC calibration failed, using raw values");
        cali_handle = NULL;
    }
    
    ESP_LOGI(TAG, "✓ MQ135 Smoke Detector initialized on GPIO34");
    return ESP_OK;
}

void mq135_deinit(void) {
    if (cali_handle) {
        adc_cali_delete_scheme_line_fitting(cali_handle);
    }
    // REMOVED: adc_oneshot_del_unit - main.c owns the ADC handle
}

uint32_t mq135_get_raw_adc(adc_oneshot_unit_handle_t adc_handle) {
    int adc_raw;
    uint32_t adc_sum = 0;
    
    // Oversample for stability (64 samples)
    for (int i = 0; i < 64; i++) {
        adc_oneshot_read(adc_handle, MQ135_ADC_CHANNEL, &adc_raw);
        adc_sum += adc_raw;
    }
    
    return adc_sum / 64;
}

float mq135_get_voltage(adc_oneshot_unit_handle_t adc_handle) {
    int adc_raw = mq135_get_raw_adc(adc_handle);
    int voltage_mv = 0;
    
    if (cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
    } else {
        // Fallback calculation if calibration unavailable
        voltage_mv = (adc_raw * 3300) / 4095;
    }
    
    return voltage_mv / 1000.0;  // Convert to volts
}

float mq135_get_ppm(adc_oneshot_unit_handle_t adc_handle) {
    int adc_raw = mq135_get_raw_adc(adc_handle);
    int voltage_mv = 0;
    
    if (cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
    } else {
        voltage_mv = (adc_raw * 3300) / 4095;
    }
    
    if (voltage_mv == 0) voltage_mv = 1;
    
    // Calculate sensor resistance using 5V circuit voltage (typical for MQ135)
    float rs = ((5000.0 * MQ135_RL) / voltage_mv) - MQ135_RL;
    if (rs < 0) rs = 0.1;
    
    // Calculate ratio (Rs/Ro)
    float ro = 76.63;  // Typical Ro for MQ135 in clean air
    float ratio = rs / ro;
    
    // Calculate smoke concentration in PPM using curve parameters
    float ppm = PARA_A * pow(ratio, PARA_B);
    
    return ppm;
}


smoke_level_t mq135_get_smoke_level(adc_oneshot_unit_handle_t adc_handle) {
    float ppm = mq135_get_ppm(adc_handle);
    
    if (ppm >= MQ135_FIRE_THRESHOLD) {
        return SMOKE_FIRE_ALERT;
    } else if (ppm >= MQ135_WARNING_THRESHOLD) {
        return SMOKE_WARNING;
    } else if (ppm >= MQ135_DETECTION_THRESHOLD) {
        return SMOKE_DETECTED;
    } else {
        return SMOKE_CLEAR;
    }
}

bool mq135_is_smoke_detected(adc_oneshot_unit_handle_t adc_handle) {
    return (mq135_get_smoke_level(adc_handle) != SMOKE_CLEAR);
}

const char* mq135_get_status_string(smoke_level_t level) {
    switch(level) {
        case SMOKE_CLEAR:       return "CLEAR";
        case SMOKE_DETECTED:    return "SMOKE DETECTED";
        case SMOKE_WARNING:     return "HEAVY SMOKE";
        case SMOKE_FIRE_ALERT:  return "FIRE ALERT";
        default:                return "UNKNOWN";
    }
}

