#include "buzzer_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";

esp_err_t buzzer_init(void) {
    // Configure GPIO as output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO");
        return ret;
    }
    
    // Start with buzzer OFF
    gpio_set_level(BUZZER_GPIO, 0);
    
    ESP_LOGI(TAG, "Active buzzer initialized on GPIO23");
    return ESP_OK;
}

void buzzer_on(void) {
    gpio_set_level(BUZZER_GPIO, 1);
    ESP_LOGD(TAG, "Buzzer ON");
}

void buzzer_off(void) {
    gpio_set_level(BUZZER_GPIO, 0);
    ESP_LOGD(TAG, "Buzzer OFF");
}

void buzzer_beep(uint32_t duration_ms) {
    buzzer_on();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_off();
}

void buzzer_alarm_pattern(int level) {
    switch(level) {
        case 0:  // Clear - Short confirmation beep
            ESP_LOGI(TAG, "Status: CLEAR");
            buzzer_beep(200);
            break;
            
        case 1:  // Smoke detected - Double beep warning
            ESP_LOGW(TAG, "Alarm: SMOKE DETECTED");
            buzzer_beep(300);
            vTaskDelay(pdMS_TO_TICKS(200));
            buzzer_beep(300);
            break;
            
        case 2:  // Heavy smoke - Rapid triple beep
            ESP_LOGW(TAG, "Alarm: HEAVY SMOKE WARNING");
            for (int i = 0; i < 3; i++) {
                buzzer_beep(400);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
            break;
            
        case 3:  // Fire alert - Continuous urgent alarm
            ESP_LOGE(TAG, "FIRE ALARM ACTIVATED!");
            for (int i = 0; i < 8; i++) {
                buzzer_beep(200);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown alarm level: %d", level);
            break;
    }
}
