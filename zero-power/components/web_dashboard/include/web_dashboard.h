#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include "esp_err.h"
#include <stdbool.h>

#define FAN_GPIO  25
#define LED_GPIO  26

esp_err_t web_dashboard_init(void);
void fan_set_state(bool state);
void led_set_state(bool state);
bool fan_get_state(void);
bool led_get_state(void);

// Update sensor data for dashboard
void dashboard_update_data(float ppm, int battery_percent, float battery_voltage, 
                          bool occupied, const char* smoke_status);

#endif

