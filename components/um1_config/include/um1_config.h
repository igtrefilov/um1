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
} lan_config_t;

typedef struct {
    bool enabled;
    char ssid[32];
    char password[64];
    char authmode[16];
    char mode[8];
} wifi_config_ap_t;

typedef struct {
    int baudrate;
    char parity[8];
    int stop_bits;
} um1_uart_config_t;

typedef struct {
    bool enabled;
    char broker[64];
    char username[32];
    char password[32];
    bool tx_enabled;
} mqtt_config_t;

typedef struct {
    bool enabled;
    char server[64];
    int port;
} stream_config_t;

typedef struct {
    bool enabled;
    char server_ip[64];
    int sync_interval_sec;
} sntp_config_t;

extern lan_config_t global_lan_config;
extern wifi_config_ap_t global_wifi_config;
extern um1_uart_config_t global_uart_config[2];
extern mqtt_config_t global_mqtt_config;
extern stream_config_t global_tcp_config;
extern stream_config_t global_udp_config;
extern sntp_config_t global_sntp_config;

void read_config_and_apply(void);

#endif // UM1_CONFIG_H
