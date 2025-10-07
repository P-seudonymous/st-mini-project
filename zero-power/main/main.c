#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mq135_driver.h"
#include "buzzer_driver.h"

static const char *TAG = "FIRE_ALARM";

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║      SMART FIRE & SMOKE DETECTION SYSTEM      ║");
    ESP_LOGI(TAG, "║      ESP32-WROOM + MQ135 + Active Buzzer      ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // Initialize MQ135 smoke sensor
    esp_err_t ret = mq135_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQ135 sensor!");
        return;
    }
    
    // Initialize buzzer
    ret = buzzer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize buzzer!");
        return;
    }
    
    // System ready beep sequence
    ESP_LOGI(TAG, "System initializing...");
    buzzer_beep(100);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_beep(100);
    
    // Sensor warm-up phase (critical for MQ135)
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Warming up MQ135 smoke sensor...");
    ESP_LOGI(TAG, "  Please wait 60 seconds for sensor stabilization");
    ESP_LOGI(TAG, "   (First time use requires 24-48 hours preheat)");
    ESP_LOGI(TAG, "");
    
    for (int i = 60; i > 0; i--) {
        if (i % 10 == 0 || i <= 5) {
            ESP_LOGI(TAG, "   Countdown: %d seconds...", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  System ready! Fire detection active!");
    ESP_LOGI(TAG, "");
    
    // Ready confirmation
    buzzer_beep(200);
    vTaskDelay(pdMS_TO_TICKS(150));
    buzzer_beep(200);
    
    smoke_level_t last_level = SMOKE_CLEAR;
    uint32_t reading_count = 0;
    bool alarm_active = false;
    
    ESP_LOGI(TAG, "╔════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║          MONITORING STARTED...             ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    while (1) {
        reading_count++;
        
        // Read sensor data
        uint32_t raw_adc = mq135_get_raw_adc();
        float voltage = mq135_get_voltage();
        float ppm = mq135_get_ppm();
        smoke_level_t level = mq135_get_smoke_level();
        bool smoke_present = mq135_is_smoke_detected();
        
        // Display readings
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "━━━━━━━━━━━ Reading #%lu ━━━━━━━━━━━", reading_count);
        ESP_LOGI(TAG, "   Raw ADC       : %lu", raw_adc);
        ESP_LOGI(TAG, "   Voltage       : %.2f V", voltage);
        ESP_LOGI(TAG, "   Concentration : %.0f PPM", ppm);
        ESP_LOGI(TAG, "   Status        : %s", mq135_get_status_string(level));
        
        // Detailed status information
        switch(level) {
            case SMOKE_CLEAR:
                ESP_LOGI(TAG, "   Environment is SAFE");
                break;
                
            case SMOKE_DETECTED:
                ESP_LOGW(TAG, "    SMOKE DETECTED!");
                ESP_LOGW(TAG, "   Check for fire sources");
                break;
                
            case SMOKE_WARNING:
                ESP_LOGW(TAG, "    HEAVY SMOKE WARNING!");
                ESP_LOGW(TAG, "  → Evacuate immediately if fire is present");
                break;
                
            case SMOKE_FIRE_ALERT:
                ESP_LOGE(TAG, "   FIRE ALERT! ");
                ESP_LOGE(TAG, "  → EVACUATE NOW!");
                ESP_LOGE(TAG, "  → CALL EMERGENCY SERVICES!");
                break;
        }
        
        ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        ESP_LOGI(TAG, "");
        
        // Alarm logic
        if (level != last_level) {
            // Status changed
            if (smoke_present && !alarm_active) {
                // Smoke just detected - trigger alarm
                ESP_LOGW(TAG, " ACTIVATING ALARM SYSTEM!");
                alarm_active = true;
                
                // Trigger appropriate alarm pattern
                buzzer_alarm_pattern(level);
                
            } else if (!smoke_present && alarm_active) {
                // Smoke cleared - deactivate alarm
                ESP_LOGI(TAG, " Air cleared - Alarm deactivated");
                alarm_active = false;
                buzzer_beep(500);  // All clear beep
            } else if (smoke_present && level != last_level) {
                // Smoke level changed while alarm is active
                ESP_LOGW(TAG, "  Smoke level changed!");
                buzzer_alarm_pattern(level);
            }
        } else if (alarm_active && level >= SMOKE_WARNING) {
            // Continue periodic alarm for warning/fire levels
            buzzer_alarm_pattern(level);
        }
        
        last_level = level;
        
        // Read every 3 seconds for fire detection
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

