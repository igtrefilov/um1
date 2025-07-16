#include "include/um1_config.h"

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
        bool dhcp = cJSON_GetObjectItem(lan, "dhcp")->valueint;
        const char *ip = cJSON_GetObjectItem(lan, "static_ip")->valuestring;
        const char *subnet = cJSON_GetObjectItem(lan, "subnet")->valuestring;
        const char *gateway = cJSON_GetObjectItem(lan, "gateway")->valuestring;
        const char *mode = cJSON_GetObjectItem(lan, "mode")->valuestring;

        ESP_LOGI("CONFIG", "LAN config: dhcp=%d, ip=%s, subnet=%s, gateway=%s, mode=%s", dhcp, ip, subnet, gateway, mode);

        //apply_lan_config(dhcp, ip, subnet, gateway, mode);
    }


    cJSON_Delete(root);
    free(data);
}

