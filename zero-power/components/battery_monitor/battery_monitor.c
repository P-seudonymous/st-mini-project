#include "battery_monitor.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "BATTERY";
// REMOVE THIS LINE: static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

esp_err_t battery_monitor_init(adc_oneshot_unit_handle_t adc_handle) {
    esp_err_t ret;
    
    // REMOVE all adc_oneshot_new_unit code
    // ONLY configure the channel
    adc_oneshot_chan_cfg_t config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_WIDTH,
    };
    ret = adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        return ret;
    }
    
    // Initialize ADC calibration
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_WIDTH,
    };
    
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  Battery monitor calibrated");
    } else {
        ESP_LOGW(TAG, "  Calibration failed, using raw values");
        cali_handle = NULL;
    }
    
    ESP_LOGI(TAG, "  Battery monitor initialized on GPIO35");
    ESP_LOGI(TAG, "  Voltage divider: 3.3kΩ + 1kΩ (ratio: 4.3)");
    return ESP_OK;
}

float battery_get_voltage(adc_oneshot_unit_handle_t adc_handle) {
    int adc_raw = 0;
    uint32_t adc_sum = 0;
    
    // Oversample for stability
    for (int i = 0; i < 32; i++) {
        adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
        adc_sum += adc_raw;
    }
    adc_raw = adc_sum / 32;
    
    int voltage_mv = 0;
    if (cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
    } else {
        voltage_mv = (adc_raw * 3300) / 4095;
    }
    
    float battery_voltage = (voltage_mv / 1000.0f) * VOLTAGE_DIVIDER_RATIO;
    return battery_voltage;
}

uint8_t battery_get_percentage(adc_oneshot_unit_handle_t adc_handle) {
    float voltage = battery_get_voltage(adc_handle);
    
    if (voltage > BATTERY_VOLTAGE_MAX) voltage = BATTERY_VOLTAGE_MAX;
    if (voltage < BATTERY_VOLTAGE_MIN) voltage = BATTERY_VOLTAGE_MIN;
    
    float percentage = ((voltage - BATTERY_VOLTAGE_MIN) / 
                       (BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN)) * 100.0f;
    
    return (uint8_t)percentage;
}

bool battery_is_charging(adc_oneshot_unit_handle_t adc_handle) {
    return (battery_get_voltage(adc_handle) > 4.1f);
}

const char* battery_get_status_string(adc_oneshot_unit_handle_t adc_handle) {
    uint8_t percentage = battery_get_percentage(adc_handle);
    bool charging = battery_is_charging(adc_handle);
    
    if (charging) {
        return "CHARGING";
    } else if (percentage > 80) {
        return "FULL";
    } else if (percentage > 50) {
        return "GOOD";
    } else if (percentage > 20) {
        return "LOW";
    } else {
        return "CRITICAL";
    }
}

