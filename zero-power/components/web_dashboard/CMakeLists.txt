idf_component_register(SRCS "web_dashboard.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver nvs_flash esp_wifi esp_netif esp_http_server)

