#include "include/um1_config.h"
#include "um1_auth.h"

lan_config_t global_lan_config;
wifi_config_ap_t global_wifi_config;
um1_uart_config_t global_uart_config[2];
mqtt_config_t global_mqtt_config;
stream_config_t global_tcp_config;
stream_config_t global_udp_config;
sntp_config_t global_sntp_config;

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

    cJSON *auth = cJSON_GetObjectItem(root, "auth");
    if (auth) {
        cJSON *u = cJSON_GetObjectItem(auth, "username");
        cJSON *p = cJSON_GetObjectItem(auth, "password");
        if (u && p) {
            auth_config.username = strdup(u->valuestring);
            auth_config.password = strdup(p->valuestring);
        }
    }

    cJSON *lan = cJSON_GetObjectItem(root, "lan");
    if (lan) {
        global_lan_config.dhcp = cJSON_GetObjectItem(lan, "dhcp")->valueint;
        strcpy(global_lan_config.static_ip, cJSON_GetObjectItem(lan, "static_ip")->valuestring);
        strcpy(global_lan_config.subnet, cJSON_GetObjectItem(lan, "subnet")->valuestring);
        strcpy(global_lan_config.gateway, cJSON_GetObjectItem(lan, "gateway")->valuestring);

        ESP_LOGI("CONFIG", "LAN config: dhcp=%d, ip=%s, subnet=%s, gateway=%s",
                 global_lan_config.dhcp,
                 global_lan_config.static_ip,
                 global_lan_config.subnet,
                 global_lan_config.gateway);
    }

    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {
        global_wifi_config.enabled = cJSON_GetObjectItem(wifi, "enabled")->valueint;
        strcpy(global_wifi_config.ssid, cJSON_GetObjectItem(wifi, "ssid")->valuestring);
        strcpy(global_wifi_config.password, cJSON_GetObjectItem(wifi, "password")->valuestring);
        strcpy(global_wifi_config.authmode, cJSON_GetObjectItem(wifi, "authmode")->valuestring);
        strcpy(global_wifi_config.mode, cJSON_GetObjectItem(wifi, "mode")->valuestring);

        ESP_LOGI("CONFIG", "WiFi config: enabled=%d, ssid=%s, password=%s, authmode=%s, mode=%s",
                 global_wifi_config.enabled,
                 global_wifi_config.ssid,
                 global_wifi_config.password,
				 global_wifi_config.authmode,
                 global_wifi_config.mode);
    }

	cJSON *uart1 = cJSON_GetObjectItem(root, "uart1");
	if (uart1) {
		global_uart_config[0].baudrate = cJSON_GetObjectItem(uart1, "baudrate")->valueint;
		strcpy(global_uart_config[0].parity, cJSON_GetObjectItem(uart1, "parity")->valuestring);
		global_uart_config[0].stop_bits = cJSON_GetObjectItem(uart1, "stop_bits")->valueint;
	}

	cJSON *uart2 = cJSON_GetObjectItem(root, "uart2");
	if (uart2) {
		global_uart_config[1].baudrate = cJSON_GetObjectItem(uart2, "baudrate")->valueint;
		strcpy(global_uart_config[1].parity, cJSON_GetObjectItem(uart2, "parity")->valuestring);
		global_uart_config[1].stop_bits = cJSON_GetObjectItem(uart2, "stop_bits")->valueint;
	}

	cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
	if (mqtt) {
	    global_mqtt_config.enabled = cJSON_GetObjectItem(mqtt, "enabled")->valueint;
	    strcpy(global_mqtt_config.broker, cJSON_GetObjectItem(mqtt, "broker")->valuestring);
	    strcpy(global_mqtt_config.username, cJSON_GetObjectItem(mqtt, "username")->valuestring);
	    strcpy(global_mqtt_config.password, cJSON_GetObjectItem(mqtt, "password")->valuestring);

	    ESP_LOGI("CONFIG", "MQTT: enabled=%d, broker=%s, user=%s",
	             global_mqtt_config.enabled,
	             global_mqtt_config.broker,
	             global_mqtt_config.username);
	}
	cJSON *tcp = cJSON_GetObjectItem(root, "tcp");
	if (tcp) {
	    global_tcp_config.enabled = cJSON_GetObjectItem(tcp, "enabled")->valueint;
	    strcpy(global_tcp_config.server, cJSON_GetObjectItem(tcp, "server")->valuestring);
	    global_tcp_config.port = cJSON_GetObjectItem(tcp, "port")->valueint;
	}

	cJSON *udp = cJSON_GetObjectItem(root, "udp");
	if (udp) {
	    global_udp_config.enabled = cJSON_GetObjectItem(udp, "enabled")->valueint;
	    strcpy(global_udp_config.server, cJSON_GetObjectItem(udp, "server")->valuestring);
	    global_udp_config.port = cJSON_GetObjectItem(udp, "port")->valueint;
	}
	cJSON *sntp = cJSON_GetObjectItem(root, "sntp");
	if (sntp) {
	    global_sntp_config.enabled = cJSON_GetObjectItem(sntp, "enabled")->valueint;
	    strcpy(global_sntp_config.server_ip, cJSON_GetObjectItem(sntp, "server_ip")->valuestring);
	    global_sntp_config.sync_interval_sec = cJSON_GetObjectItem(sntp, "sync_interval_sec")->valueint;

	    ESP_LOGI("CONFIG", "SNTP: enabled=%d, server_ip=%s, interval=%d",
	             global_sntp_config.enabled,
	             global_sntp_config.server_ip,
	             global_sntp_config.sync_interval_sec);
	}

    cJSON_Delete(root);
    free(data);
}

void save_config(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *auth = cJSON_CreateObject();
    cJSON_AddStringToObject(auth, "username", auth_config.username);
    cJSON_AddStringToObject(auth, "password", auth_config.password);
    cJSON_AddItemToObject(root, "auth", auth);

    cJSON *lan = cJSON_CreateObject();
    cJSON_AddBoolToObject(lan, "dhcp", global_lan_config.dhcp);
    cJSON_AddStringToObject(lan, "static_ip", global_lan_config.static_ip);
    cJSON_AddStringToObject(lan, "subnet", global_lan_config.subnet);
    cJSON_AddStringToObject(lan, "gateway", global_lan_config.gateway);
    cJSON_AddItemToObject(root, "lan", lan);

    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "enabled", global_wifi_config.enabled);
    cJSON_AddStringToObject(wifi, "ssid", global_wifi_config.ssid);
    cJSON_AddStringToObject(wifi, "password", global_wifi_config.password);
    cJSON_AddStringToObject(wifi, "authmode", global_wifi_config.authmode);
    cJSON_AddStringToObject(wifi, "mode", global_wifi_config.mode);
    cJSON_AddItemToObject(root, "wifi", wifi);

    cJSON *uart1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(uart1, "baudrate", global_uart_config[0].baudrate);
    cJSON_AddStringToObject(uart1, "parity", global_uart_config[0].parity);
    cJSON_AddNumberToObject(uart1, "stop_bits", global_uart_config[0].stop_bits);
    cJSON_AddItemToObject(root, "uart1", uart1);

    cJSON *uart2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(uart2, "baudrate", global_uart_config[1].baudrate);
    cJSON_AddStringToObject(uart2, "parity", global_uart_config[1].parity);
    cJSON_AddNumberToObject(uart2, "stop_bits", global_uart_config[1].stop_bits);
    cJSON_AddItemToObject(root, "uart2", uart2);

    cJSON *mqtt = cJSON_CreateObject();
    cJSON_AddBoolToObject(mqtt, "enabled", global_mqtt_config.enabled);
    cJSON_AddStringToObject(mqtt, "broker", global_mqtt_config.broker);
    cJSON_AddStringToObject(mqtt, "username", global_mqtt_config.username);
    cJSON_AddStringToObject(mqtt, "password", global_mqtt_config.password);
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    cJSON *tcp = cJSON_CreateObject();
    cJSON_AddBoolToObject(tcp, "enabled", global_tcp_config.enabled);
    cJSON_AddStringToObject(tcp, "server", global_tcp_config.server);
    cJSON_AddNumberToObject(tcp, "port", global_tcp_config.port);
    cJSON_AddItemToObject(root, "tcp", tcp);

    cJSON *udp = cJSON_CreateObject();
    cJSON_AddBoolToObject(udp, "enabled", global_udp_config.enabled);
    cJSON_AddStringToObject(udp, "server", global_udp_config.server);
    cJSON_AddNumberToObject(udp, "port", global_udp_config.port);
    cJSON_AddItemToObject(root, "udp", udp);

    cJSON *sntp = cJSON_CreateObject();
    cJSON_AddBoolToObject(sntp, "enabled", global_sntp_config.enabled);
    cJSON_AddStringToObject(sntp, "server_ip", global_sntp_config.server_ip);
    cJSON_AddNumberToObject(sntp, "sync_interval_sec", global_sntp_config.sync_interval_sec);
    cJSON_AddItemToObject(root, "sntp", sntp);

    char *out = cJSON_Print(root);
    FILE *f = fopen("/spiffs/src/config.json", "w");
    if (f) {
        fwrite(out, 1, strlen(out), f);
        fclose(f);
    } else {
        ESP_LOGE("CONFIG", "Failed to open config.json for writing");
    }
    free(out);
    cJSON_Delete(root);
}

