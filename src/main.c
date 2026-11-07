#include <stdio.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "led_strip.h"a
#include "esp_log.h"
#include "mdns.h"

#define WIFI_SSID "325rocker26"
#define WIFI_PASS "rocker1L7P"


static led_strip_handle_t strip;
// Hexcode, last chosen value for display in root page
static uint32_t current_rgb = 0x00ff00;

//todo: if page needs to store anything in flash, update sdkconfig and code
static esp_err_t root_handler(httpd_req_t *req)
//root page, has a color picker, and a button to set the LED color
{
    //Todo: Clean this up, add features
    char page[300];
    snprintf(page, sizeof(page),
        "<h1>Hello</h1>"
        "<form action=\"/color\">"
        "<input type=\"color\" name=\"c\" value=\"#%06x\">"
        "<button>Set LED</button>"
        "</form>", (unsigned)current_rgb);
    httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW('TAG', "Disconnected, retrying...");
        esp_wifi_connect();
    } 
}

static esp_err_t color_handler(httpd_req_t *req)
{
    char query[32], value[16];
    unsigned int rgb;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "c", value, sizeof(value)) == ESP_OK &&
        sscanf(value, "%%23%x", &rgb) == 1) {

        rgb &= 0xFFFFFF;
        current_rgb = rgb;

        // set_pixel takes (r, g, b); >>3 dims to 1/8 brightness
        led_strip_set_pixel(strip, 0,
                            ((rgb >> 8) & 0xFF) >> 3,
                            ((rgb >> 16) & 0xFF) >> 3,
                            (rgb & 0xFF) >> 3);
        led_strip_refresh(strip);
    }

    // Redirect so refreshing doesn't resubmit the color
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


void app_main(void)
//on esp32 startyp
{
    
    //rmt device, required for RGB LED
    //gpio port 38, allocates memory for 1 led, 
    led_strip_config_t led_cfg = { .strip_gpio_num = 38, .max_leds = 1 };
    led_strip_rmt_config_t rmt = { .resolution_hz = 10 * 1000 * 1000 };
    led_strip_new_rmt_device(&led_cfg, &rmt, &strip);


    //wifi configs, default ip: 192.168.4.1
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta()); 

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler, NULL));

    wifi_config_t wifi = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));  
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp32"));
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);


    httpd_handle_t server = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &cfg);

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
    httpd_uri_t col = { .uri = "/color", .method = HTTP_GET, .handler = color_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &col);
}