#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

static const char *TAG = "OLED";

#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_FREQ_HZ   400000

static ssd1306_handle_t display;

void app_main(void) {
    ESP_LOGI(TAG, "Starting OLED test...");
    
    // Initialize I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
    
    ESP_LOGI(TAG, "I2C initialized");
    
    // Initialize display
    display = ssd1306_create(I2C_NUM_0, 0x3C);
    
    if (display == NULL) {
        ESP_LOGE(TAG, "OLED failed - check wiring!");
        return;
    }
    
    ESP_LOGI(TAG, "OLED working!");
    
    ssd1306_clear_screen(display, 0x00);
    
    // Display "Hello World"
    ssd1306_draw_string(display, 15, 10, (const uint8_t *)"Hello World!", 16, 1);
    ssd1306_draw_string(display, 20, 35, (const uint8_t *)"OLED Works", 16, 1);
    ssd1306_refresh_gram(display);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Counter loop
    int count = 0;
    while (1) {
        char text[32];
        
        ssd1306_clear_screen(display, 0x00);
        
        snprintf(text, sizeof(text), "Count: %d", count);
        ssd1306_draw_string(display, 25, 25, (const uint8_t *)text, 16, 1);
        
        ssd1306_refresh_gram(display);
        
        ESP_LOGI(TAG, "Count: %d", count);
        
        count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

