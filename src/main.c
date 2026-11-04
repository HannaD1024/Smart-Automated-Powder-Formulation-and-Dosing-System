#include <stdio.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "led_strip.h"

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
        "<input type=\"color\" name=\"c\" value=\"%s\">"
        "<button>Set LED</button>"
        "</form>", current_rgb);
    httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t color_handler(httpd_req_t *req)
//changes led color, redirects to root page
{
    char query[32], value[16];
    unsigned int rgb;

    // The browser sends /color?c=%23rrggbb  (%23 is the URL-encoded '#')
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "c", value, sizeof(value)) == ESP_OK &&
        sscanf(value, "%%23%x", &rgb) == 1) {

        //
        rgb &= 0xFFFFFF;
        current_rgb = rgb;

        
        led_strip_set_pixel(strip, 0,
                            ((rgb >> 8) & 0xFF) >> 3,
                            ((rgb >> 16) & 0xFF) >> 3,
                            (rgb & 0xFF) >> 3);
        led_strip_refresh(strip);
    }

    return root_handler(req);

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
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init);

    //wifi name, password, and only one phone at a time
    wifi_config_t wifi = {
        .ap = {
            .ssid = "ESP32-Hello",
            .password = "12345678",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    //start wifi
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi);
    esp_wifi_start();

    httpd_handle_t server = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &cfg);

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_handler };
    httpd_uri_t col = { .uri = "/color", .method = HTTP_GET, .handler = color_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &col);
}