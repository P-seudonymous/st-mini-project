#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "mq135_driver.h"
#include "buzzer_driver.h"
#include "battery_monitor.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

static const char *TAG = "FIRE_ALARM";
static ssd1306_handle_t ssd1306_dev = NULL;

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║              FIRE DETECTION SYSTEM            ║");
    ESP_LOGI(TAG, "║       ESP32 + MQ135 + Battery Monitor + OLED  ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // CREATE ADC1 HANDLE ONCE
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &adc1_handle));
    ESP_LOGI(TAG, "ADC1 unit initialized");
    ESP_LOGI(TAG, "");
    
    // Initialize peripherals
    ESP_LOGI(TAG, "Initializing peripherals...");
    
    // Initialize OLED display directly
    ESP_LOGI(TAG, "Initializing SSD1306 OLED...");
    ssd1306_dev = ssd1306_create(I2C_NUM_0, 0x3C);
    if (ssd1306_dev == NULL) {
        ESP_LOGE(TAG, "Failed to create SSD1306 device");
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_refresh_gram(ssd1306_dev);
    ESP_LOGI(TAG, "OLED initialized successfully");
    
    ESP_ERROR_CHECK(mq135_init(adc1_handle));
    ESP_ERROR_CHECK(buzzer_init());
    ESP_ERROR_CHECK(battery_monitor_init(adc1_handle));
    
    // Display splash screen
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_draw_string(ssd1306_dev, 20, 15, (const uint8_t *)"FIRE", 16, 1);
    ssd1306_draw_string(ssd1306_dev, 10, 35, (const uint8_t *)"DETECTOR", 16, 1);
    ssd1306_refresh_gram(ssd1306_dev);
    
    buzzer_beep(100);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_beep(100);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // MQ135 sensor warm-up
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Warming up MQ135 sensor...");
    ESP_LOGI(TAG, "Please wait 60 seconds for stabilization");
    
    for (int i = 60; i > 0; i--) {
        if (i % 10 == 0 || i <= 5) {
            ESP_LOGI(TAG, "Countdown: %d seconds...", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "System ready! Monitoring started.");
    ESP_LOGI(TAG, "");
    
    buzzer_beep(200);
    vTaskDelay(pdMS_TO_TICKS(150));
    buzzer_beep(200);
    
    smoke_level_t last_level = SMOKE_CLEAR;
    uint32_t reading_count = 0;
    bool alarm_active = false;
    
    ESP_LOGI(TAG, "╔════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║        CONTINUOUS MONITORING ACTIVE        ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // Main monitoring loop
    while (1) {
        reading_count++;
        
        // Read all sensors
        float ppm = mq135_get_ppm(adc1_handle);
        smoke_level_t level = mq135_get_smoke_level(adc1_handle);
        uint8_t battery_percent = battery_get_percentage(adc1_handle);
        float battery_voltage = battery_get_voltage(adc1_handle);
        bool is_charging = battery_is_charging(adc1_handle);
        
        // Update OLED display - battery info
        char line1[32], line2[32], line3[32];
        
        ssd1306_clear_screen(ssd1306_dev, 0x00);
        
        snprintf(line1, sizeof(line1), "BAT: %d%%", battery_percent);
        ssd1306_draw_string(ssd1306_dev, 25, 10, (const uint8_t *)line1, 16, 1);
        
        snprintf(line2, sizeof(line2), "%.2fV", battery_voltage);
        ssd1306_draw_string(ssd1306_dev, 35, 30, (const uint8_t *)line2, 16, 1);
        
        const char *status;
        if (is_charging) {
            status = "CHARGING";
        } else if (battery_percent > 80) {
            status = "FULL";
        } else if (battery_percent > 50) {
            status = "GOOD";
        } else if (battery_percent > 20) {
            status = "LOW";
        } else {
            status = "CRITICAL";
        }
        
        ssd1306_draw_string(ssd1306_dev, 20, 50, (const uint8_t *)status, 16, 1);
        ssd1306_refresh_gram(ssd1306_dev);
        
        // Log data to serial monitor
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "━━━━━━━━━━ Reading #%lu ━━━━━━━━━━", reading_count);
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "  SMOKE SENSOR:");
        ESP_LOGI(TAG, "     PPM Value    : %.0f", ppm);
        ESP_LOGI(TAG, "     Status       : %s", mq135_get_status_string(level));
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "  BATTERY:");
        ESP_LOGI(TAG, "     Percentage   : %d%%", battery_percent);
        ESP_LOGI(TAG, "     Voltage      : %.2fV", battery_voltage);
        ESP_LOGI(TAG, "     Status       : %s", battery_get_status_string(adc1_handle));
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        ESP_LOGI(TAG, "");
        
        // Smoke detection alarm logic
        if (level != last_level) {
            if (level >= SMOKE_DETECTED && !alarm_active) {
                ESP_LOGW(TAG, "");
                ESP_LOGW(TAG, "ALARM ACTIVATED!");
                ESP_LOGW(TAG, "");
                alarm_active = true;
                buzzer_alarm_pattern(level);
                
            } else if (level == SMOKE_CLEAR && alarm_active) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "Air cleared - Alarm deactivated");
                ESP_LOGI(TAG, "");
                alarm_active = false;
                buzzer_beep(500);
                
            } else if (level >= SMOKE_DETECTED && level != last_level) {
                ESP_LOGW(TAG, "");
                ESP_LOGW(TAG, "Smoke level escalated!");
                ESP_LOGW(TAG, "");
                buzzer_alarm_pattern(level);
            }
        } else if (alarm_active && level >= SMOKE_WARNING) {
            buzzer_alarm_pattern(level);
        }
        
        // Battery low warning
        if (battery_percent < 20 && !is_charging) {
            ESP_LOGW(TAG, "LOW BATTERY WARNING!");
            ESP_LOGW(TAG, "Please recharge soon!");
        }
        
        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

