#include "web_dashboard.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "DASHBOARD";

// Access Point credentials - change these if you want
#define AP_SSID      "shunya"
#define AP_PASSWORD  "ilovecps"

static httpd_handle_t server = NULL;
static bool fan_state = false;
static bool led_state = false;

// Global sensor data
static float g_ppm = 0;
static int g_battery_percent = 0;
static float g_battery_voltage = 0;
static bool g_occupied = false;
static char g_smoke_status[32] = "CLEAR";

void dashboard_update_data(float ppm, int battery_percent, float battery_voltage, 
                          bool occupied, const char* smoke_status) {
    g_ppm = ppm;
    g_battery_percent = battery_percent;
    g_battery_voltage = battery_voltage;
    g_occupied = occupied;
    strncpy(g_smoke_status, smoke_status, sizeof(g_smoke_status) - 1);
}

void fan_set_state(bool state) {
    fan_state = state;
    gpio_set_level(FAN_GPIO, state ? 1 : 0);
    ESP_LOGI(TAG, "Fan turned %s", state ? "ON" : "OFF");
}

void led_set_state(bool state) {
    led_state = state;
    gpio_set_level(LED_GPIO, state ? 1 : 0);
    ESP_LOGI(TAG, "LED turned %s", state ? "ON" : "OFF");
}

bool fan_get_state(void) {
    return fan_state;
}

bool led_get_state(void) {
    return led_state;
}

// HTML Dashboard with modern UI
static const char* html_page = 
"<!DOCTYPE html><html><head>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<meta charset='UTF-8'>"
"<title>Fire Alarm Dashboard</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:linear-gradient(135deg,#667eea 0%%,#764ba2 100%%);min-height:100vh;padding:20px;color:#fff}"
".container{max-width:800px;margin:0 auto}"
"h1{text-align:center;margin-bottom:30px;font-size:2.5em;text-shadow:2px 2px 4px rgba(0,0,0,0.3)}"
".dashboard{background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border-radius:20px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
".sensor-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:20px;margin-bottom:30px}"
".sensor-card{background:rgba(255,255,255,0.15);padding:25px;border-radius:15px;text-align:center;transition:transform 0.3s,box-shadow 0.3s}"
".sensor-card:hover{transform:translateY(-5px);box-shadow:0 10px 20px rgba(0,0,0,0.2)}"
".sensor-icon{font-size:3em;margin-bottom:10px}"
".sensor-value{font-size:2.5em;font-weight:bold;margin:10px 0}"
".sensor-label{font-size:0.9em;opacity:0.9;text-transform:uppercase;letter-spacing:1px}"
".status-good{color:#4ade80}"
".status-warning{color:#fbbf24}"
".status-danger{color:#f87171}"
".control-section{background:rgba(255,255,255,0.1);border-radius:15px;padding:25px;margin-top:20px}"
".control-title{font-size:1.5em;margin-bottom:20px;text-align:center}"
".controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px}"
".btn{padding:20px;font-size:1.1em;border:none;border-radius:12px;cursor:pointer;transition:all 0.3s;font-weight:600;text-transform:uppercase;letter-spacing:1px}"
".btn-on{background:linear-gradient(135deg,#10b981,#059669);color:white;box-shadow:0 4px 15px rgba(16,185,129,0.4)}"
".btn-off{background:linear-gradient(135deg,#ef4444,#dc2626);color:white;box-shadow:0 4px 15px rgba(239,68,68,0.4)}"
".btn:hover{transform:scale(1.05);box-shadow:0 6px 20px rgba(0,0,0,0.3)}"
".btn:active{transform:scale(0.98)}"
".footer{text-align:center;margin-top:30px;opacity:0.8;font-size:0.9em}"
"@media(max-width:600px){h1{font-size:1.8em}.sensor-value{font-size:2em}}"
"</style>"
"<script>"
"function toggle(device){"
"fetch('/control?device='+device).then(()=>setTimeout(()=>location.reload(),300));"
"}"
"setInterval(()=>location.reload(),5000);"
"</script></head><body>"
"<div class='container'>"
"<h1>🔥 Fire Alarm Dashboard</h1>"
"<div class='dashboard'>"
"<div class='sensor-grid'>"
"<div class='sensor-card'>"
"<div class='sensor-icon'>💨</div>"
"<div class='sensor-label'>Gas Level</div>"
"<div class='sensor-value %s'>%.0f</div>"
"<div class='sensor-label'>PPM - %s</div>"
"</div>"
"<div class='sensor-card'>"
"<div class='sensor-icon'>🔋</div>"
"<div class='sensor-label'>Battery</div>"
"<div class='sensor-value %s'>%d%%</div>"
"<div class='sensor-label'>%.2fV</div>"
"</div>"
"<div class='sensor-card'>"
"<div class='sensor-icon'>%s</div>"
"<div class='sensor-label'>Room Status</div>"
"<div class='sensor-value'>%s</div>"
"</div>"
"</div>"
"<div class='control-section'>"
"<div class='control-title'>⚙️ Controls</div>"
"<div class='controls'>"
"<button class='btn %s' onclick='toggle(\"fan\")'>🌀 Fan<br>%s</button>"
"<button class='btn %s' onclick='toggle(\"led\")'>💡 LED<br>%s</button>"
"</div>"
"</div>"
"</div>"
"<div class='footer'>Smart Fire Detection System | ESP32 Powered</div>"
"</div></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    char response[4096];
    
    const char *ppm_class = (g_ppm > 400) ? "status-danger" : (g_ppm > 200) ? "status-warning" : "status-good";
    const char *bat_class = (g_battery_percent < 20) ? "status-danger" : (g_battery_percent < 50) ? "status-warning" : "status-good";
    const char *room_icon = g_occupied ? "👤" : "🚫";
    
    snprintf(response, sizeof(response), html_page,
        ppm_class, g_ppm, g_smoke_status,
        bat_class, g_battery_percent, g_battery_voltage,
        room_icon, g_occupied ? "OCCUPIED" : "EMPTY",
        fan_state ? "btn-on" : "btn-off", fan_state ? "ON" : "OFF",
        led_state ? "btn-on" : "btn-off", led_state ? "ON" : "OFF"
    );
    
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t control_handler(httpd_req_t *req) {
    char query[100];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char device[32];
        if (httpd_query_key_value(query, "device", device, sizeof(device)) == ESP_OK) {
            if (strcmp(device, "fan") == 0) {
                fan_set_state(!fan_state);
            } else if (strcmp(device, "led") == 0) {
                led_set_state(!led_state);
            }
        }
    }
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t control_uri = {
            .uri = "/control",
            .method = HTTP_GET,
            .handler = control_handler
        };
        httpd_register_uri_handler(server, &control_uri);
        
        ESP_LOGI(TAG, "Web server started");
    }
    return server;
}

esp_err_t web_dashboard_init(void) {
    // Initialize GPIO for fan and LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FAN_GPIO) | (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(FAN_GPIO, 0);
    gpio_set_level(LED_GPIO, 0);
    
    ESP_LOGI(TAG, "✓ Fan GPIO%d and LED GPIO%d initialized", FAN_GPIO, LED_GPIO);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize networking
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create AP (Access Point)
    esp_netif_create_default_wifi_ap();
    
    // WiFi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Set WiFi mode to AP only
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    
    // Configure AP

   
        wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };
    
    // Static IP is set by default for AP mode (192.168.4.1)
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());



    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║      Access Point Started Successfully    ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📶 WiFi Network: %s", AP_SSID);
    ESP_LOGI(TAG, "🔐 Password: %s", AP_PASSWORD);
    ESP_LOGI(TAG, "📱 Dashboard IP: http://192.168.4.1");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📌 Steps to connect:");
    ESP_LOGI(TAG, "  1. Open WiFi settings on your phone/laptop");
    ESP_LOGI(TAG, "  2. Find network: %s", AP_SSID);
    ESP_LOGI(TAG, "  3. Password: %s", AP_PASSWORD);
    ESP_LOGI(TAG, "  4. Open browser and go to http://192.168.4.1");
    ESP_LOGI(TAG, "");
    
    // Start web server
    start_webserver();
    
    return ESP_OK;
}

