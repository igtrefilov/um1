#include "um1_wifi.h"

#define TAG "um1_wifi"
#define TCP_PORT 4444
#define LISTEN_BACKLOG 5

static esp_netif_t *wifi_netif = NULL;

/* ===== AP MODE ONLY: TCP Server ===== */
static void wifi_tcp_server_task(void *pvParameters)
{
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(wifi_netif, &ip_info));

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = ip_info.ip.addr,
        .sin_port = htons(TCP_PORT),
    };

    int err = bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    err = listen(listen_sock, LISTEN_BACKLOG);
    if (err < 0) {
        ESP_LOGE(TAG, "Error during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t socklen = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &socklen);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        ESP_LOGI(TAG, "Accepted connection from %s:%d",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        const char *resp = "Hello from ESP32 WiFi AP TCP server!\r\n";
        send(client_sock, resp, strlen(resp), 0);
        close(client_sock);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

/* ===== Event Handlers ===== */
static void wifi_event_handler_ap(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: Station " MACSTR " left, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP started");
        xTaskCreate(wifi_tcp_server_task, "tcp_ap", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
    }
}

static void wifi_event_handler_sta(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "STA: Connected to AP");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA: Disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/* ===== Entry Point ===== */

static wifi_auth_mode_t get_auth_mode(const char *auth_str)
{
    if (strcmp(auth_str, "wpa2") == 0) {
        return WIFI_AUTH_WPA2_PSK;
    } else if (strcmp(auth_str, "wpa3") == 0) {
        return WIFI_AUTH_WPA3_PSK;
    } else if (strcmp(auth_str, "wpa_wpa2") == 0) {
        return WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        return WIFI_AUTH_OPEN;
    }
}

void start_wifi(void)
{
    if (!global_wifi_config.enabled) {
        ESP_LOGW(TAG, "Wi-Fi is disabled in config");
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_auth_mode_t auth_mode = get_auth_mode(global_wifi_config.authmode);

    if (strcmp(global_wifi_config.mode, "sta") == 0) {
        esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (ap_netif) {
            esp_netif_dhcps_stop(ap_netif);
            esp_netif_destroy(ap_netif);
            ESP_LOGI(TAG, "AP interface removed to ensure clean STA start");
        }

        wifi_netif = esp_netif_create_default_wifi_sta();

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_sta, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler_sta, NULL, NULL));

        wifi_config_t wifi_config = { 0 };
        strncpy((char *)wifi_config.sta.ssid, global_wifi_config.ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, global_wifi_config.password, sizeof(wifi_config.sta.password));
        wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifi_config.sta.threshold.authmode = auth_mode;
        wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());

        ESP_LOGI(TAG, "Started in STA mode. SSID: %s, Authmode: %d", global_wifi_config.ssid, auth_mode);
    } else {
        wifi_netif = esp_netif_create_default_wifi_ap();

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_ap, NULL, NULL));

        wifi_config_t wifi_config = { 0 };
        strncpy((char *)wifi_config.ap.ssid, global_wifi_config.ssid, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen(global_wifi_config.ssid);
        strncpy((char *)wifi_config.ap.password, global_wifi_config.password, sizeof(wifi_config.ap.password));
        wifi_config.ap.channel = 1;
        wifi_config.ap.max_connection = 4;
        wifi_config.ap.authmode = strlen(global_wifi_config.password) == 0 ? WIFI_AUTH_OPEN : auth_mode;
        wifi_config.ap.pmf_cfg.required = true;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "Started in AP mode. SSID: %s, Authmode: %d", global_wifi_config.ssid, wifi_config.ap.authmode);
    }
}
