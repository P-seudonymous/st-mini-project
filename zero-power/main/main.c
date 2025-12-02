#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/i2c.h"
#include "mq135_driver.h"
#include "buzzer_driver.h"
#include "battery_monitor.h"
#include "pir_sensor.h"
#include "web_dashboard.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

static const char *TAG = "FIRE_ALARM";
static ssd1306_handle_t ssd1306_dev = NULL;

#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_FREQ_HZ   400000

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║          SMART FIRE DETECTION SYSTEM          ║");
    ESP_LOGI(TAG, "║   ESP32 + MQ135 + PIR + Solar + Web Control   ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // INITIALIZE I2C FIRST
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
    ESP_LOGI(TAG, "✓ I2C initialized");
    
    // CREATE ADC1 HANDLE
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &adc1_handle));
    ESP_LOGI(TAG, "✓ ADC1 unit initialized");
    ESP_LOGI(TAG, "");
    
    // Initialize peripherals
    ESP_LOGI(TAG, "Initializing peripherals...");
    
    // Initialize OLED display
    ssd1306_dev = ssd1306_create(I2C_NUM_0, 0x3C);
    if (ssd1306_dev == NULL) {
        ESP_LOGE(TAG, "Failed to create SSD1306 device");
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_refresh_gram(ssd1306_dev);
    ESP_LOGI(TAG, "✓ OLED display");
    
    ESP_ERROR_CHECK(mq135_init(adc1_handle));
    ESP_ERROR_CHECK(buzzer_init());
    ESP_ERROR_CHECK(battery_monitor_init(adc1_handle));
    ESP_ERROR_CHECK(pir_init());
    ESP_LOGI(TAG, "");
    
    // Initialize WiFi and Web Dashboard
    ESP_LOGI(TAG, "Starting web dashboard...");
    ESP_ERROR_CHECK(web_dashboard_init());
    
    // Display splash screen
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_draw_string(ssd1306_dev, 20, 10, (const uint8_t *)"SMART", 16, 1);
    ssd1306_draw_string(ssd1306_dev, 25, 30, (const uint8_t *)"FIRE", 16, 1);
    ssd1306_draw_string(ssd1306_dev, 5, 50, (const uint8_t *)"DETECTOR", 16, 1);
    ssd1306_refresh_gram(ssd1306_dev);
    
    buzzer_beep(100);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_beep(100);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // MQ135 sensor warm-up with countdown on display
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Warming up MQ135 sensor...");
    ESP_LOGI(TAG, "Please wait 60 seconds for stabilization");
    
    for (int i = 60; i > 0; i--) {
        char countdown_text[32];
        
        ssd1306_clear_screen(ssd1306_dev, 0x00);
        ssd1306_draw_string(ssd1306_dev, 10, 10, (const uint8_t *)"WARMING UP", 16, 1);
        
        snprintf(countdown_text, sizeof(countdown_text), "%d sec", i);
        
        // Center the countdown number
        int text_width = (i >= 10) ? 40 : 32;
        int x_pos = (128 - text_width) / 2;
        ssd1306_draw_string(ssd1306_dev, x_pos, 35, (const uint8_t *)countdown_text, 16, 1);
        ssd1306_refresh_gram(ssd1306_dev);
        
        if (i % 10 == 0 || i <= 5) {
            ESP_LOGI(TAG, "  Countdown: %d seconds...", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "✓ System ready! Monitoring started.");
    ESP_LOGI(TAG, "");
    
    // Ready screen
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_draw_string(ssd1306_dev, 30, 25, (const uint8_t *)"READY!", 16, 1);
    ssd1306_refresh_gram(ssd1306_dev);
    
    buzzer_beep(200);
    vTaskDelay(pdMS_TO_TICKS(150));
    buzzer_beep(200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
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
        bool occupied = pir_is_occupied();
        
        // Update web dashboard with current sensor data
        dashboard_update_data(ppm, battery_percent, battery_voltage, 
                            occupied, mq135_get_status_string(level));
        
        // Update OLED display with split screen layout
        char gas_ppm[16], bat_percent[16], bat_voltage[16];
        
        ssd1306_clear_screen(ssd1306_dev, 0x00);
        
        // LEFT SIDE - GAS SENSOR (x: 0-63)
        ssd1306_draw_string(ssd1306_dev, 15, 5, (const uint8_t *)"GAS", 16, 1);
        
        // PPM value (centered)
        snprintf(gas_ppm, sizeof(gas_ppm), "%.0f", ppm);
        int ppm_width = (ppm >= 100) ? 24 : (ppm >= 10) ? 16 : 8;
        ssd1306_draw_string(ssd1306_dev, (63 - ppm_width) / 2, 25, (const uint8_t *)gas_ppm, 16, 1);
        
        // PPM label
        ssd1306_draw_string(ssd1306_dev, 18, 45, (const uint8_t *)"PPM", 16, 1);
        
        // VERTICAL LINE IN MIDDLE (x: 64)
        ssd1306_draw_line(ssd1306_dev, 64, 0, 64, 55);
        
        // RIGHT SIDE - BATTERY (x: 65-127)
        ssd1306_draw_string(ssd1306_dev, 75, 5, (const uint8_t *)"BAT", 16, 1);
        
        // Battery percentage (centered)
        snprintf(bat_percent, sizeof(bat_percent), "%d%%", battery_percent);
        int bat_width = (battery_percent >= 100) ? 24 : (battery_percent >= 10) ? 16 : 8;
        ssd1306_draw_string(ssd1306_dev, 65 + (63 - bat_width) / 2, 25, (const uint8_t *)bat_percent, 16, 1);
        
        // Voltage
        snprintf(bat_voltage, sizeof(bat_voltage), "%.2fV", battery_voltage);
        ssd1306_draw_string(ssd1306_dev, 72, 45, (const uint8_t *)bat_voltage, 12, 1);
        
        // PIR status at bottom (centered across full width)
        const char *room_status = occupied ? "OCCUPIED" : "EMPTY";
        int status_width = occupied ? 64 : 40;
        ssd1306_draw_string(ssd1306_dev, (128 - status_width) / 2, 56, 
                           (const uint8_t *)room_status, 12, 1);
        
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
        ESP_LOGI(TAG, "  ROOM STATUS:");
        ESP_LOGI(TAG, "     Occupancy    : %s", room_status);
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "  WEB CONTROLS:");
        ESP_LOGI(TAG, "     Fan          : %s", fan_get_state() ? "ON" : "OFF");
        ESP_LOGI(TAG, "     LED          : %s", led_get_state() ? "ON" : "OFF");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        ESP_LOGI(TAG, "");
        
        // Smoke detection alarm logic
        if (level != last_level) {
            if (level >= SMOKE_DETECTED && !alarm_active) {
                ESP_LOGW(TAG, "");
                ESP_LOGW(TAG, "⚠️ ⚠️ ⚠️  FIRE ALARM ACTIVATED  ⚠️ ⚠️ ⚠️");
                ESP_LOGW(TAG, "");
                alarm_active = true;
                
                // Show alarm on display
                ssd1306_clear_screen(ssd1306_dev, 0x00);
                ssd1306_draw_string(ssd1306_dev, 20, 10, (const uint8_t *)"FIRE", 16, 1);
                ssd1306_draw_string(ssd1306_dev, 15, 35, (const uint8_t *)"ALARM!", 16, 1);
                ssd1306_refresh_gram(ssd1306_dev);
                
                buzzer_alarm_pattern(level);
                
            } else if (level == SMOKE_CLEAR && alarm_active) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "✓ Air cleared - Alarm deactivated");
                ESP_LOGI(TAG, "");
                alarm_active = false;
                buzzer_beep(500);
                
            } else if (level >= SMOKE_DETECTED && level != last_level) {
                ESP_LOGW(TAG, "");
                ESP_LOGW(TAG, "⚠️ Smoke level escalated to: %s", mq135_get_status_string(level));
                ESP_LOGW(TAG, "");
                buzzer_alarm_pattern(level);
            }
        } else if (alarm_active && level >= SMOKE_WARNING) {
            buzzer_alarm_pattern(level);
        }
        
        // Battery low warning
        if (battery_percent < 20 && !is_charging) {
            ESP_LOGW(TAG, "⚠️ LOW BATTERY - Please recharge soon!");
        }
        
        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

