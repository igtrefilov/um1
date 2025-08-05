#include "um1_sntp.h"

static const char *TAG = "um1_sntp";

static bool sntp_initialized = false;
static bool time_synced = false;
static int64_t ntp_base_time_us = 0;
static int64_t sync_time_monotonic_us = 0;

void time_sync_notification_cb(struct timeval *tv) {
    ntp_base_time_us = ((int64_t)tv->tv_sec * 1000000LL) + tv->tv_usec;
    sync_time_monotonic_us = esp_timer_get_time();
    time_synced = true;
    ESP_LOGI(TAG, "Time synchronized: %llu", ntp_base_time_us);
}

void start_sntp(void)
{
    if (!global_sntp_config.enabled) {
        ESP_LOGI(TAG, "SNTP disabled in config");
        return;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, global_sntp_config.server_ip);
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_set_sync_interval(global_sntp_config.sync_interval_sec * 1000);
    esp_sntp_init();

    sntp_initialized = true;
    ESP_LOGI(TAG, "SNTP started with server %s, interval: %d sec", global_sntp_config.server_ip, global_sntp_config.sync_interval_sec);
}

bool is_sntp_enabled(void) {
    return global_sntp_config.enabled && time_synced;
}

uint64_t get_ntp_time_us(void) {
    if (!is_sntp_enabled()) return 0;

    int64_t now = esp_timer_get_time();
    return ntp_base_time_us + (now - sync_time_monotonic_us);
}
