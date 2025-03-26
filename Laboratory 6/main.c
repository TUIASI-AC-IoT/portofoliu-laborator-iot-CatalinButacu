/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "soft-ap.h"
#include "http-server.h"
#include "driver/gpio.h"
#include "../mdns/include/mdns.h"

#define RESET_BUTTON GPIO_NUM_2

#define DEFAULT_SCAN_LIST_SIZE 5
#define NVS_COMPARE_KEY_PARAM "nvs"
#define NVS_AP_ESP_WIFI_SSID_KEY "nvsApSsid"
#define NVS_AP_ESP_WIFI_PASS_KEY "nvsApPass"
#define NVS_STA_ESP_WIFI_SSID_KEY "nvsStaSsid"
#define NVS_STA_ESP_WIFI_PASS_KEY "nvsStaPass"
#define NVS_STA_AP_DEFAULT_MODE_KEY "nvsApStaMode"
#define NVS_WIFI_CONNECT_MODE_STA "modeSta"
#define NVS_WIFI_CONNECT_MODE_AP "modeAp"

#define NVS_WIFI_RESTART_KEY "Wifi_Restart"
#define NVS_WIFI_RESTART_VALUE_RESTART "restart"
#define NVS_WIFI_RESTART_VALUE_WRITE "write"

#define NVS_STORAGE_NAME "storage"

static const char *TAG = "main";
static const char *TAG_SCAN = "scan";

static char *SSIDs = "";
esp_err_t init_sta(const char *ssid, const char *password);

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG_SCAN, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG_SCAN, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG_SCAN, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_LOGI(TAG_SCAN, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG_SCAN, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG_SCAN, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG_SCAN, "RSSI \t\t%d", ap_info[i].rssi);
        //add_ssid(ap_info[i].ssid);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG_SCAN, "Channel \t\t%d\n", ap_info[i].primary);
    }
    printf("%s", SSIDs);

}

esp_err_t nvs_wifi_connect(void) 
{
    char nvs_ssid[32] = {0};
    char nvs_password[64] = {0};
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Read stored SSID and password
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        size_t ssid_len = sizeof(nvs_ssid), pass_len = sizeof(nvs_password);
        if (nvs_get_str(nvs_handle, "ssid", nvs_ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs_handle, "pass", nvs_password, &pass_len) == ESP_OK) {
            nvs_close(nvs_handle);
            // Credentials found, start STA mode
            return init_sta(nvs_ssid, nvs_password);
        }
        nvs_close(nvs_handle);
    }

    // No stored credentials, start provisioning mode (SoftAP)
    wifi_init_softap();
    start_webserver();
    return ESP_ERR_INVALID_ARG;
}

void reset_nvs_task(void *arg) {
    gpio_reset_pin(RESET_BUTTON);
    gpio_set_direction(RESET_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RESET_BUTTON, GPIO_PULLUP_ONLY);
    while (1) {
        if (gpio_get_level(RESET_BUTTON) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            if (gpio_get_level(RESET_BUTTON) == 0) {
                ESP_LOGI(TAG, "Resetting Wi-Fi credentials...");
                nvs_handle_t nvs_handle;
                if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
                    nvs_erase_key(nvs_handle, "ssid");
                    nvs_erase_key(nvs_handle, "pass");
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                }
                esp_restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    char nvs_ssid[32] = {0};
    char nvs_password[64] = {0};
    nvs_handle_t nvs_handle;
    bool has_credentials = false;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        size_t ssid_len = sizeof(nvs_ssid), pass_len = sizeof(nvs_password);
        if (nvs_get_str(nvs_handle, "ssid", nvs_ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs_handle, "pass", nvs_password, &pass_len) == ESP_OK) {
            has_credentials = true;
        }
        nvs_close(nvs_handle);
    }
    if (has_credentials) {
        ESP_LOGI(TAG, "Connecting to stored SSID: %s", nvs_ssid);
        init_sta(nvs_ssid, nvs_password);
        mdns_init();
    } else {
        ESP_LOGI(TAG, "Starting SoftAP mode for provisioning");
        wifi_init_softap();
        start_webserver();
    }
    xTaskCreate(reset_nvs_task, "reset_nvs_task", 4096, NULL, 5, NULL);
}