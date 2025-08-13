// um1_wifi.c
#include "um1_wifi.h"

#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/inet.h"   // inet_addr

static esp_netif_t *wifi_ap_netif  = NULL;
static esp_netif_t *wifi_sta_netif = NULL;

static TaskHandle_t ap_tcp_task  = NULL, ap_udp_task  = NULL;
static TaskHandle_t sta_tcp_task = NULL, sta_udp_task = NULL;

static const char *TAG = "um1_wifi";

#ifndef ESP_ERROR_CHECK_WITHOUT_ABORT
#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) do {                           \
    esp_err_t __e = (x);                                                \
    if (__e != ESP_OK) ESP_LOGE(TAG, "%s -> %s", #x, esp_err_to_name(__e)); \
} while (0)
#endif

#ifndef HAVE_STRCASECMP_FALLBACK
#define HAVE_STRCASECMP_FALLBACK
static int _ascii_tolower(int c){ return (c>='A' && c<='Z') ? (c+32) : c; }
static int strcasecmp_fallback(const char *a, const char *b){
    while (*a && *b){
        int da = _ascii_tolower((unsigned char)*a);
        int db = _ascii_tolower((unsigned char)*b);
        if (da != db) return da - db;
        ++a; ++b;
    }
    return _ascii_tolower((unsigned char)*a) - _ascii_tolower((unsigned char)*b);
}
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#define STRCASECMP strcasecmp
#else
#define STRCASECMP(a,b) strcasecmp_fallback((a),(b))
#endif

#define AP_CHANNEL_DEFAULT   1
#define AP_MAX_CONN_DEFAULT  4

static inline bool is_valid_ssid(const char *s) {
    size_t n = strlen(s);
    return n > 0 && n <= 32;
}

static inline bool is_valid_wpa_pass(const char *p) {
    size_t n = strlen(p);
    return n >= 8 && n <= 63;
}

static inline void fill_pmf_open(wifi_pmf_config_t *pmf) {
    pmf->capable  = true;
    pmf->required = false;
}

static inline void fill_pmf_protected(wifi_pmf_config_t *pmf) {
    pmf->capable  = true;
    pmf->required = true;
}

static wifi_auth_mode_t norm_auth_from_str(const char *s) {
    if (!s) return WIFI_AUTH_OPEN;
    if (STRCASECMP(s, "wpa3")     == 0) return WIFI_AUTH_WPA3_PSK;
    if (STRCASECMP(s, "wpa2")     == 0) return WIFI_AUTH_WPA2_PSK;
    if (STRCASECMP(s, "wpa_wpa2") == 0) return WIFI_AUTH_WPA_WPA2_PSK;
    return WIFI_AUTH_OPEN;
}

/* ===== Server tasks ===== */

static void ap_tcp_server_task(void *pv) {
    ESP_LOGI(TAG, "AP TCP server task start");
    run_tcp_server(wifi_ap_netif, AP_TCP_PORT);
    ap_tcp_task = NULL;
    vTaskDelete(NULL);
}

static void ap_udp_server_task(void *pv) {
    ESP_LOGI(TAG, "AP UDP server task start");
    run_udp_server(wifi_ap_netif, AP_UDP_PORT);
    ap_udp_task = NULL;
    vTaskDelete(NULL);
}

static void sta_tcp_server_task(void *pv) {
    ESP_LOGI(TAG, "STA TCP server task start");
    run_tcp_server(wifi_sta_netif, STA_TCP_PORT);
    sta_tcp_task = NULL;
    vTaskDelete(NULL);
}

static void sta_udp_server_task(void *pv) {
    ESP_LOGI(TAG, "STA UDP server task start");
    run_udp_server(wifi_sta_netif, STA_UDP_PORT);
    sta_udp_task = NULL;
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
        if (ap_tcp_task == NULL)
            xTaskCreate(ap_tcp_server_task, "ap_tcp_server", 4096, NULL, tskIDLE_PRIORITY + 5, &ap_tcp_task);
        if (ap_udp_task == NULL)
            xTaskCreate(ap_udp_server_task, "ap_udp_server", 4096, NULL, tskIDLE_PRIORITY + 5, &ap_udp_task);
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
        if (sta_tcp_task == NULL)
            xTaskCreate(sta_tcp_server_task, "sta_tcp_server", 4096, NULL, tskIDLE_PRIORITY + 5, &sta_tcp_task);
        if (sta_udp_task == NULL)
            xTaskCreate(sta_udp_server_task, "sta_udp_server", 4096, NULL, tskIDLE_PRIORITY + 5, &sta_udp_task);
    }
}


static esp_err_t apply_ap_config_checked(const wifi_if_config_t *cfg_in)
{
    if (!is_valid_ssid(cfg_in->ssid)) {
        ESP_LOGE(TAG, "AP: invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t ap = (wifi_config_t){0};
    strlcpy((char*)ap.ap.ssid, cfg_in->ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(cfg_in->ssid);

    wifi_auth_mode_t auth = norm_auth_from_str(cfg_in->authmode);
    size_t pwlen = strlen(cfg_in->password);

    if (auth == WIFI_AUTH_OPEN || pwlen == 0) {
        ap.ap.authmode = WIFI_AUTH_OPEN;
        ap.ap.password[0] = '\0';
        fill_pmf_open(&ap.ap.pmf_cfg);
    } else {
        if (!is_valid_wpa_pass(cfg_in->password)) {
            ESP_LOGE(TAG, "AP: invalid password length (8..63 required)");
            return ESP_ERR_INVALID_ARG;
        }
        strlcpy((char*)ap.ap.password, cfg_in->password, sizeof(ap.ap.password));
        ap.ap.authmode = auth;
        fill_pmf_protected(&ap.ap.pmf_cfg);
    }

    ap.ap.channel = AP_CHANNEL_DEFAULT;
    ap.ap.max_connection = AP_MAX_CONN_DEFAULT;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static esp_err_t apply_sta_config_checked(const wifi_if_config_t *cfg_in)
{
    if (!is_valid_ssid(cfg_in->ssid)) {
        ESP_LOGE(TAG, "STA: invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t sta = (wifi_config_t){0};
    strlcpy((char*)sta.sta.ssid, cfg_in->ssid, sizeof(sta.sta.ssid));

    wifi_auth_mode_t auth = norm_auth_from_str(cfg_in->authmode);
    size_t pwlen = strlen(cfg_in->password);

    if (auth == WIFI_AUTH_OPEN || pwlen == 0) {
        sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
        sta.sta.password[0] = '\0';
    } else {
        if (!is_valid_wpa_pass(cfg_in->password)) {
            ESP_LOGE(TAG, "STA: invalid password length (8..63 required)");
            return ESP_ERR_INVALID_ARG;
        }
        strlcpy((char*)sta.sta.password, cfg_in->password, sizeof(sta.sta.password));
        sta.sta.threshold.authmode = auth;
        sta.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    }

    sta.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &sta);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(STA) failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}


static void apply_ap_netif_ip_config(const wifi_if_config_t *cfg)
{
    if (!cfg->dhcp) {
        ESP_LOGI(TAG, "AP: static ip=%s subnet=%s gw=%s", cfg->static_ip, cfg->subnet, cfg->gateway);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(wifi_ap_netif));
        esp_netif_ip_info_t ip = {0};
        ip.ip.addr      = inet_addr(cfg->static_ip);
        ip.netmask.addr = inet_addr(cfg->subnet);
        ip.gw.addr      = inet_addr(cfg->gateway);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_ip_info(wifi_ap_netif, &ip));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(wifi_ap_netif));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(wifi_ap_netif)); // обычно уже запущен
    }
}

static void apply_sta_netif_ip_config(const wifi_if_config_t *cfg)
{
    if (!cfg->dhcp) {
        ESP_LOGI(TAG, "STA: static ip=%s subnet=%s gw=%s", cfg->static_ip, cfg->subnet, cfg->gateway);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcpc_stop(wifi_sta_netif));
        esp_netif_ip_info_t ip = {0};
        ip.ip.addr      = inet_addr(cfg->static_ip);
        ip.netmask.addr = inet_addr(cfg->subnet);
        ip.gw.addr      = inet_addr(cfg->gateway);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_ip_info(wifi_sta_netif, &ip));
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcpc_start(wifi_sta_netif));
    }
}

esp_err_t start_wifi(void)
{
    if (!global_wifi_config.enabled) {
        ESP_LOGW(TAG, "Wi-Fi disabled");
        return ESP_OK;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        return err;
    }

    bool do_ap  = (STRCASECMP(global_wifi_config.mode, "ap")   == 0) ||
                  (STRCASECMP(global_wifi_config.mode, "apsta")== 0);
    bool do_sta = (STRCASECMP(global_wifi_config.mode, "sta")  == 0) ||
                  (STRCASECMP(global_wifi_config.mode, "apsta")== 0);

    wifi_mode_t mode = do_ap && do_sta ? WIFI_MODE_APSTA : (do_ap ? WIFI_MODE_AP : WIFI_MODE_STA);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(mode));

    if (do_ap) {
        wifi_ap_netif = esp_netif_create_default_wifi_ap();
        apply_ap_netif_ip_config(&global_wifi_config.ap);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_ap, NULL, NULL));

        err = apply_ap_config_checked(&global_wifi_config.ap);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "AP config invalid; AP not started");
            do_ap = false;
        }
    }

    if (do_sta) {
        wifi_sta_netif = esp_netif_create_default_wifi_sta();
        apply_sta_netif_ip_config(&global_wifi_config.sta);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_sta, NULL, NULL));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler_sta, NULL, NULL));

        err = apply_sta_config_checked(&global_wifi_config.sta);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "STA config invalid; STA not started");
            do_sta = false;
        }
    }

    if (!do_ap && !do_sta) {
        ESP_LOGE(TAG, "Neither AP nor STA could be started due to invalid config");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_deinit());
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
    if (do_sta) ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());

    ESP_LOGI(TAG, "WiFi started mode=%s", global_wifi_config.mode);
    return ESP_OK;
}
