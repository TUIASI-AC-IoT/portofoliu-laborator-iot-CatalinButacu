#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/event_groups.h"

#include "esp_http_server.h"

#define MAX_AP_ENTRIES 10  
uint8_t *ssids[MAX_AP_ENTRIES];
uint8_t pos = 0;

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    const char resp[] = "<html>"
                        "<body>"
                        "<form action='/results.html' target='_blank' method='post'>"
                        "<label for='fname'>Networks found:</label>"
                        "<br>"
                        "<select name='ssid'>"
                        "<option value='ssid-exemplu-1'>ssid-exemplu-1</option>"
                        "<option value='ssid-exemplu-2'>ssid-exemplu-2</option>"
                        "<option value='ssid-exemplu-3'>ssid-exemplu-3</option>"
                        "<option value='ssid-exemplu-4'>ssid-exemplu-4</option>"
                        "</select>"
                        "<br>"
                        "<label for='ipass'>Security key:</label><br>"
                        "<input type='password' name='ipass'><br>"
                        "<input type='submit' value='Submit'>"
                        "</form>"
                        "</body>"
                        "</html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);  // TODO: Trimitere sir de caractere ce contine pagina web prezentata in laborator (lista populata cu rezultatele scanarii)
    return ESP_OK;
}

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
    char content[100];  // Buffer to store received data

    // Truncate if content length is larger than buffer
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    int ret = httpd_req_recv(req, content, recv_size);

    if (ret <= 0) {  
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    content[recv_size] = '\0'; // Null-terminate received data

    // Extract SSID and password using httpd_query_key_value()
    char ssid[32] = {0}, password[64] = {0};
    httpd_query_key_value(content, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(content, "pass", password, sizeof(password));

    // Store in NVS
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "pass", password));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    // Restart the device to apply changes
    esp_restart();

    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/index.html",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* URI handler structure for POST /uri */
httpd_uri_t uri_post = {
    .uri      = "/results.html",
    .method   = HTTP_POST,
    .handler  = post_handler,
    .user_ctx = NULL
};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}