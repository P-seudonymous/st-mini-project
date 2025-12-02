#ifndef PIR_SENSOR_H
#define PIR_SENSOR_H

#include "esp_err.h"
#include <stdbool.h>

#define PIR_GPIO 18

esp_err_t pir_init(void);
bool pir_is_occupied(void);

#endif

