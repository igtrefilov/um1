#ifndef UM1_CONFIG_H
#define UM1_CONFIG_H

#include <esp_log.h>
#include <string.h>
#include "cJSON.h"
#include "esp_spiffs.h"

typedef struct {
    bool dhcp;
    char static_ip[16];
    char subnet[16];
    char gateway[16];
    char mode[16];
} lan_config_t;

typedef struct {
    bool enabled;
    char ssid[32];
    char password[64];
} wifi_config_ap_t;

extern lan_config_t global_lan_config;
extern wifi_config_ap_t global_wifi_config;

void read_config_and_apply(void);

#endif // UM1_CONFIG_H
