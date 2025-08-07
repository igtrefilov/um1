#include "um1_ssh.h"
#include "um1_config.h"
#include "um1_auth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cJSON.h"

static const char *TAG = "um1_ssh";

static bool check_credentials(const char *user, const char *pass) {
    if (!auth_config.username || !auth_config.password) return false;
    return strcmp(user, auth_config.username) == 0 &&
           strcmp(pass, auth_config.password) == 0;
}

static void print_json(cJSON *obj) {
    if (!obj) return;
    char *out = cJSON_Print(obj);
    if (out) {
        ESP_LOGI(TAG, "%s", out);
        free(out);
    }
    cJSON_Delete(obj);
}

static void cmd_get(const char *target) {
    cJSON *obj = cJSON_CreateObject();
    if (strcmp(target, "lan") == 0) {
        cJSON_AddBoolToObject(obj, "dhcp", global_lan_config.dhcp);
        cJSON_AddStringToObject(obj, "static_ip", global_lan_config.static_ip);
        cJSON_AddStringToObject(obj, "subnet", global_lan_config.subnet);
        cJSON_AddStringToObject(obj, "gateway", global_lan_config.gateway);
    } else if (strcmp(target, "wifi") == 0) {
        cJSON_AddBoolToObject(obj, "enabled", global_wifi_config.enabled);
        cJSON_AddStringToObject(obj, "ssid", global_wifi_config.ssid);
        cJSON_AddStringToObject(obj, "password", global_wifi_config.password);
        cJSON_AddStringToObject(obj, "authmode", global_wifi_config.authmode);
        cJSON_AddStringToObject(obj, "mode", global_wifi_config.mode);
    } else if (strcmp(target, "uart1") == 0) {
        cJSON_AddNumberToObject(obj, "baudrate", global_uart_config[0].baudrate);
        cJSON_AddStringToObject(obj, "parity", global_uart_config[0].parity);
        cJSON_AddNumberToObject(obj, "stop_bits", global_uart_config[0].stop_bits);
    } else if (strcmp(target, "uart2") == 0) {
        cJSON_AddNumberToObject(obj, "baudrate", global_uart_config[1].baudrate);
        cJSON_AddStringToObject(obj, "parity", global_uart_config[1].parity);
        cJSON_AddNumberToObject(obj, "stop_bits", global_uart_config[1].stop_bits);
    } else if (strcmp(target, "mqtt") == 0) {
        cJSON_AddBoolToObject(obj, "enabled", global_mqtt_config.enabled);
        cJSON_AddStringToObject(obj, "broker", global_mqtt_config.broker);
        cJSON_AddStringToObject(obj, "username", global_mqtt_config.username);
        cJSON_AddStringToObject(obj, "password", global_mqtt_config.password);
    } else if (strcmp(target, "tcp") == 0) {
        cJSON_AddBoolToObject(obj, "enabled", global_tcp_config.enabled);
        cJSON_AddStringToObject(obj, "server", global_tcp_config.server);
        cJSON_AddNumberToObject(obj, "port", global_tcp_config.port);
    } else if (strcmp(target, "udp") == 0) {
        cJSON_AddBoolToObject(obj, "enabled", global_udp_config.enabled);
        cJSON_AddStringToObject(obj, "server", global_udp_config.server);
        cJSON_AddNumberToObject(obj, "port", global_udp_config.port);
    } else if (strcmp(target, "sntp") == 0) {
        cJSON_AddBoolToObject(obj, "enabled", global_sntp_config.enabled);
        cJSON_AddStringToObject(obj, "server_ip", global_sntp_config.server_ip);
        cJSON_AddNumberToObject(obj, "sync_interval_sec", global_sntp_config.sync_interval_sec);
    }
    print_json(obj);
}

static void cmd_set(const char *target, const char *field, const char *value) {
    if (strcmp(target, "lan") == 0) {
        if (strcmp(field, "dhcp") == 0) global_lan_config.dhcp = atoi(value);
        else if (strcmp(field, "static_ip") == 0) strlcpy(global_lan_config.static_ip, value, sizeof(global_lan_config.static_ip));
        else if (strcmp(field, "subnet") == 0) strlcpy(global_lan_config.subnet, value, sizeof(global_lan_config.subnet));
        else if (strcmp(field, "gateway") == 0) strlcpy(global_lan_config.gateway, value, sizeof(global_lan_config.gateway));
    } else if (strcmp(target, "wifi") == 0) {
        if (strcmp(field, "enabled") == 0) global_wifi_config.enabled = atoi(value);
        else if (strcmp(field, "ssid") == 0) strlcpy(global_wifi_config.ssid, value, sizeof(global_wifi_config.ssid));
        else if (strcmp(field, "password") == 0) strlcpy(global_wifi_config.password, value, sizeof(global_wifi_config.password));
        else if (strcmp(field, "authmode") == 0) strlcpy(global_wifi_config.authmode, value, sizeof(global_wifi_config.authmode));
        else if (strcmp(field, "mode") == 0) strlcpy(global_wifi_config.mode, value, sizeof(global_wifi_config.mode));
    } else if (strcmp(target, "uart1") == 0) {
        if (strcmp(field, "baudrate") == 0) global_uart_config[0].baudrate = atoi(value);
        else if (strcmp(field, "parity") == 0) strlcpy(global_uart_config[0].parity, value, sizeof(global_uart_config[0].parity));
        else if (strcmp(field, "stop_bits") == 0) global_uart_config[0].stop_bits = atoi(value);
    } else if (strcmp(target, "uart2") == 0) {
        if (strcmp(field, "baudrate") == 0) global_uart_config[1].baudrate = atoi(value);
        else if (strcmp(field, "parity") == 0) strlcpy(global_uart_config[1].parity, value, sizeof(global_uart_config[1].parity));
        else if (strcmp(field, "stop_bits") == 0) global_uart_config[1].stop_bits = atoi(value);
    } else if (strcmp(target, "mqtt") == 0) {
        if (strcmp(field, "enabled") == 0) global_mqtt_config.enabled = atoi(value);
        else if (strcmp(field, "broker") == 0) strlcpy(global_mqtt_config.broker, value, sizeof(global_mqtt_config.broker));
        else if (strcmp(field, "username") == 0) strlcpy(global_mqtt_config.username, value, sizeof(global_mqtt_config.username));
        else if (strcmp(field, "password") == 0) strlcpy(global_mqtt_config.password, value, sizeof(global_mqtt_config.password));
    } else if (strcmp(target, "tcp") == 0) {
        if (strcmp(field, "enabled") == 0) global_tcp_config.enabled = atoi(value);
        else if (strcmp(field, "server") == 0) strlcpy(global_tcp_config.server, value, sizeof(global_tcp_config.server));
        else if (strcmp(field, "port") == 0) global_tcp_config.port = atoi(value);
    } else if (strcmp(target, "udp") == 0) {
        if (strcmp(field, "enabled") == 0) global_udp_config.enabled = atoi(value);
        else if (strcmp(field, "server") == 0) strlcpy(global_udp_config.server, value, sizeof(global_udp_config.server));
        else if (strcmp(field, "port") == 0) global_udp_config.port = atoi(value);
    } else if (strcmp(target, "sntp") == 0) {
        if (strcmp(field, "enabled") == 0) global_sntp_config.enabled = atoi(value);
        else if (strcmp(field, "server_ip") == 0) strlcpy(global_sntp_config.server_ip, value, sizeof(global_sntp_config.server_ip));
        else if (strcmp(field, "sync_interval_sec") == 0) global_sntp_config.sync_interval_sec = atoi(value);
    }
    save_config();
}

static void process_cli(const char *line) {
    char action[8] = {0};
    char module[16] = {0};
    char field[32] = {0};
    char value[64] = {0};
    int count = sscanf(line, "%7s %15s %31s %63s", action, module, field, value);
    if (count < 2) return;
    if (strcmp(action, "get") == 0) {
        cmd_get(module);
    } else if (strcmp(action, "set") == 0 && count == 4) {
        cmd_set(module, field, value);
    }
}

static void ssh_session_task(void *arg) {
    ESP_LOGI(TAG, "SSH session handler started");
    // placeholder command processing loop
    const char *demo_user = "admin";
    const char *demo_pass = "admin";
    if (!check_credentials(demo_user, demo_pass)) {
        ESP_LOGW(TAG, "Authentication failed for SSH session");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "SSH session authenticated");
    // Example CLI usage for demonstration
    process_cli("get lan");
    vTaskDelete(NULL);
}

void start_ssh_server(void) {
    ESP_LOGI(TAG, "Starting SSH server (stub)");
    xTaskCreate(ssh_session_task, "ssh_session_task", 4096, NULL, 5, NULL);
}
