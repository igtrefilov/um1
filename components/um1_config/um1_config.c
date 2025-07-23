#include "include/um1_config.h"

lan_config_t global_lan_config;
wifi_config_ap_t global_wifi_config;

void read_config_and_apply(void)
{
    FILE *fp = fopen("/spiffs/src/config.json", "r");
    if (fp == NULL) {
        ESP_LOGE("CONFIG", "Failed to open config.json");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char *data = malloc(len + 1);
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE("CONFIG", "JSON parse error");
        free(data);
        return;
    }

    cJSON *lan = cJSON_GetObjectItem(root, "lan");
    if (lan) {
        global_lan_config.dhcp = cJSON_GetObjectItem(lan, "dhcp")->valueint;
        strcpy(global_lan_config.static_ip, cJSON_GetObjectItem(lan, "static_ip")->valuestring);
        strcpy(global_lan_config.subnet, cJSON_GetObjectItem(lan, "subnet")->valuestring);
        strcpy(global_lan_config.gateway, cJSON_GetObjectItem(lan, "gateway")->valuestring);
        strcpy(global_lan_config.mode, cJSON_GetObjectItem(lan, "mode")->valuestring);

        ESP_LOGI("CONFIG", "LAN config: dhcp=%d, ip=%s, subnet=%s, gateway=%s, mode=%s",
                 global_lan_config.dhcp,
                 global_lan_config.static_ip,
                 global_lan_config.subnet,
                 global_lan_config.gateway,
                 global_lan_config.mode);
    }

    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {
        global_wifi_config.enabled = cJSON_GetObjectItem(wifi, "enabled")->valueint;
        strcpy(global_wifi_config.ssid, cJSON_GetObjectItem(wifi, "ssid")->valuestring);
        strcpy(global_wifi_config.password, cJSON_GetObjectItem(wifi, "password")->valuestring);

        ESP_LOGI("CONFIG", "WiFi config: enabled=%d, ssid=%s, password=%s",
                 global_wifi_config.enabled,
                 global_wifi_config.ssid,
                 global_wifi_config.password);
    }

    cJSON_Delete(root);
    free(data);
}

