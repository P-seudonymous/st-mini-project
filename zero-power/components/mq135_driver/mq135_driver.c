#include "mq135_driver.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "MQ135";
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

// Smoke curve parameters
#define PARA_A  605.18   
#define PARA_B  -3.937  

esp_err_t mq135_init(void) {
    esp_err_t ret;
    
    // Configure ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = MQ135_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit");
        return ret;
    }
    
    // Configure ADC channel
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
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
    }
}

uint32_t mq135_get_raw_adc(void) {
    int adc_raw;
    uint32_t adc_sum = 0;
    
    // Oversample for stability (64 samples)
    for (int i = 0; i < 64; i++) {
        adc_oneshot_read(adc_handle, MQ135_ADC_CHANNEL, &adc_raw);
        adc_sum += adc_raw;
    }
    
    return adc_sum / 64;
}

float mq135_get_voltage(void) {
    int adc_raw = mq135_get_raw_adc();
    int voltage_mv = 0;
    
    if (cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
    } else {
        // Fallback calculation if calibration unavailable
        voltage_mv = (adc_raw * 3300) / 4095;
    }
    
    return voltage_mv / 1000.0;  // Convert to volts
}

float mq135_get_ppm(void) {
    int adc_raw = mq135_get_raw_adc();
    int voltage_mv = 0;
    
    if (cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
    } else {
        voltage_mv = (adc_raw * 3300) / 4095;
    }
    
    if (voltage_mv == 0) voltage_mv = 1;
    
    // Calculate sensor resistance
    float rs = ((3300.0 * RL_VALUE) / voltage_mv) - RL_VALUE;
    if (rs < 0) rs = 0.1;
    
    // Calculate ratio (Rs/Ro)
    float ro = 76.63;  // Typical Ro for MQ135 in clean air
    float ratio = rs / ro;
    
    // Calculate smoke concentration in PPM
    float ppm = PARA_A * pow(ratio, PARA_B);
    
    return ppm;
}

smoke_level_t mq135_get_smoke_level(void) {
    float ppm = mq135_get_ppm();
    
    if (ppm >= SMOKE_THRESHOLD_HIGH) {
        return SMOKE_FIRE_ALERT;
    } else if (ppm >= SMOKE_THRESHOLD_MEDIUM) {
        return SMOKE_WARNING;
    } else if (ppm >= SMOKE_THRESHOLD_LOW) {
        return SMOKE_DETECTED;
    } else {
        return SMOKE_CLEAR;
    }
}

bool mq135_is_smoke_detected(void) {
    return (mq135_get_smoke_level() != SMOKE_CLEAR);
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

