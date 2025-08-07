#include "um1_wifi.h"

#define TAG "um1_wifi"
#define TCP_PORT 4444
#define LISTEN_BACKLOG 5

static esp_netif_t *wifi_ap_netif = NULL;
static esp_netif_t *wifi_sta_netif = NULL;

/* ===== AP MODE ONLY: TCP Server ===== */
static void wifi_tcp_server_task(void *pvParameters)
{
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(wifi_ap_netif, &ip_info));

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

    bool do_ap = strcmp(global_wifi_config.mode, "ap") == 0 || strcmp(global_wifi_config.mode, "apsta") == 0;
    bool do_sta = strcmp(global_wifi_config.mode, "sta") == 0 || strcmp(global_wifi_config.mode, "apsta") == 0;

    wifi_mode_t mode = WIFI_MODE_NULL;
    if (do_ap && do_sta) mode = WIFI_MODE_APSTA;
    else if (do_ap) mode = WIFI_MODE_AP;
    else mode = WIFI_MODE_STA;

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

    if (do_ap) {
        wifi_ap_netif = esp_netif_create_default_wifi_ap();
        if (!global_wifi_config.ap.dhcp) {
            esp_netif_dhcps_stop(wifi_ap_netif);
            esp_netif_ip_info_t ip_info = {0};
            ip_info.ip.addr = inet_addr(global_wifi_config.ap.static_ip);
            ip_info.netmask.addr = inet_addr(global_wifi_config.ap.subnet);
            ip_info.gw.addr = inet_addr(global_wifi_config.ap.gateway);
            ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_ap_netif, &ip_info));
        }
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_ap, NULL, NULL));

        wifi_config_t ap_cfg = {0};
        strncpy((char *)ap_cfg.ap.ssid, global_wifi_config.ap.ssid, sizeof(ap_cfg.ap.ssid));
        ap_cfg.ap.ssid_len = strlen(global_wifi_config.ap.ssid);
        strncpy((char *)ap_cfg.ap.password, global_wifi_config.ap.password, sizeof(ap_cfg.ap.password));
        ap_cfg.ap.channel = 1;
        ap_cfg.ap.max_connection = 4;
        wifi_auth_mode_t ap_auth = get_auth_mode(global_wifi_config.ap.authmode);
        ap_cfg.ap.authmode = strlen(global_wifi_config.ap.password) == 0 ? WIFI_AUTH_OPEN : ap_auth;
        ap_cfg.ap.pmf_cfg.required = true;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    }

    if (do_sta) {
        wifi_sta_netif = esp_netif_create_default_wifi_sta();
        if (!global_wifi_config.sta.dhcp) {
            esp_netif_dhcpc_stop(wifi_sta_netif);
            esp_netif_ip_info_t ip_info = {0};
            ip_info.ip.addr = inet_addr(global_wifi_config.sta.static_ip);
            ip_info.netmask.addr = inet_addr(global_wifi_config.sta.subnet);
            ip_info.gw.addr = inet_addr(global_wifi_config.sta.gateway);
            ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_sta_netif, &ip_info));
        }
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_sta, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler_sta, NULL, NULL));

        wifi_config_t sta_cfg = {0};
        strncpy((char *)sta_cfg.sta.ssid, global_wifi_config.sta.ssid, sizeof(sta_cfg.sta.ssid));
        strncpy((char *)sta_cfg.sta.password, global_wifi_config.sta.password, sizeof(sta_cfg.sta.password));
        sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        sta_cfg.sta.threshold.authmode = get_auth_mode(global_wifi_config.sta.authmode);
        sta_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    if (do_sta) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    ESP_LOGI(TAG, "Started WiFi mode=%s", global_wifi_config.mode);
}
