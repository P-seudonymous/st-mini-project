#ifndef BUZZER_DRIVER_H
#define BUZZER_DRIVER_H

#include "esp_err.h"
#include "driver/gpio.h"

// Buzzer Configuration
#define BUZZER_GPIO         23

// Function prototypes
esp_err_t buzzer_init(void);
void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep(uint32_t duration_ms);
void buzzer_alarm_pattern(int level);

#endif
