#include "pir_sensor.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "PIR";

esp_err_t pir_init(void) {
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << PIR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&pir_conf);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PIR sensor initialized on GPIO%d", PIR_GPIO);
    }
    return ret;
}

bool pir_is_occupied(void) {
    return gpio_get_level(PIR_GPIO) == 1;
}

