idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES mq135_driver buzzer_driver battery_monitor pir_sensor web_dashboard)

