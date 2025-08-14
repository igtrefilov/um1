#include "include/um1_config.h"
#include "um1_auth.h"

lan_config_t global_lan_config;
um1_wifi_config_t global_wifi_config;
um1_uart_config_t global_uart_config[2];
mqtt_config_t global_mqtt_config;
stream_config_t global_tcp_config;
stream_config_t global_udp_config;
sntp_config_t global_sntp_config;
ip_profile_t global_ip_profiles[2];
mqtt_profile_t global_mqtt_profiles[2];
routing_config_t global_routing_config;

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
        strcpy(global_wifi_config.mode, cJSON_GetObjectItem(wifi, "mode")->valuestring);

        cJSON *ap = cJSON_GetObjectItem(wifi, "ap");
        if (ap) {
            strcpy(global_wifi_config.ap.ssid, cJSON_GetObjectItem(ap, "ssid")->valuestring);
            strcpy(global_wifi_config.ap.password, cJSON_GetObjectItem(ap, "password")->valuestring);
            strcpy(global_wifi_config.ap.authmode, cJSON_GetObjectItem(ap, "authmode")->valuestring);
            global_wifi_config.ap.dhcp = cJSON_GetObjectItem(ap, "dhcp")->valueint;
            strcpy(global_wifi_config.ap.static_ip, cJSON_GetObjectItem(ap, "static_ip")->valuestring);
            strcpy(global_wifi_config.ap.subnet, cJSON_GetObjectItem(ap, "subnet")->valuestring);
            strcpy(global_wifi_config.ap.gateway, cJSON_GetObjectItem(ap, "gateway")->valuestring);
        }

        cJSON *sta = cJSON_GetObjectItem(wifi, "sta");
        if (sta) {
            strcpy(global_wifi_config.sta.ssid, cJSON_GetObjectItem(sta, "ssid")->valuestring);
            strcpy(global_wifi_config.sta.password, cJSON_GetObjectItem(sta, "password")->valuestring);
            strcpy(global_wifi_config.sta.authmode, cJSON_GetObjectItem(sta, "authmode")->valuestring);
            global_wifi_config.sta.dhcp = cJSON_GetObjectItem(sta, "dhcp")->valueint;
            strcpy(global_wifi_config.sta.static_ip, cJSON_GetObjectItem(sta, "static_ip")->valuestring);
            strcpy(global_wifi_config.sta.subnet, cJSON_GetObjectItem(sta, "subnet")->valuestring);
            strcpy(global_wifi_config.sta.gateway, cJSON_GetObjectItem(sta, "gateway")->valuestring);
        }

        ESP_LOGI("CONFIG", "WiFi config: enabled=%d, mode=%s",
                 global_wifi_config.enabled,
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
            cJSON *tx_en = cJSON_GetObjectItem(mqtt, "tx_enabled");
            global_mqtt_config.tx_enabled = tx_en ? tx_en->valueint : false;
            cJSON *rx_en = cJSON_GetObjectItem(mqtt, "rx_enabled");
            global_mqtt_config.rx_enabled = rx_en ? rx_en->valueint : false;
            cJSON *topic = cJSON_GetObjectItem(mqtt, "topic");
            if (topic) {
                strcpy(global_mqtt_config.topic, topic->valuestring);
            } else {
                global_mqtt_config.topic[0] = '\0';
            }

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
            cJSON *role = cJSON_GetObjectItem(tcp, "role");
            if (role) {
                strcpy(global_tcp_config.role, role->valuestring);
            } else {
                strcpy(global_tcp_config.role, "client");
            }
        }

        cJSON *udp = cJSON_GetObjectItem(root, "udp");
        if (udp) {
            global_udp_config.enabled = cJSON_GetObjectItem(udp, "enabled")->valueint;
            strcpy(global_udp_config.server, cJSON_GetObjectItem(udp, "server")->valuestring);
            global_udp_config.port = cJSON_GetObjectItem(udp, "port")->valueint;
            cJSON *role = cJSON_GetObjectItem(udp, "role");
            if (role) {
                strcpy(global_udp_config.role, role->valuestring);
            } else {
                strcpy(global_udp_config.role, "client");
            }
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

    cJSON *ip_prof = cJSON_GetObjectItem(root, "ip_profile");
    if (ip_prof) {
        cJSON *ip1 = cJSON_GetObjectItem(ip_prof, "ip1");
        if (ip1) {
            global_ip_profiles[0].client = cJSON_GetObjectItem(ip1, "client")->valueint;
            strcpy(global_ip_profiles[0].address, cJSON_GetObjectItem(ip1, "address")->valuestring);
            global_ip_profiles[0].port = cJSON_GetObjectItem(ip1, "port")->valueint;
            strcpy(global_ip_profiles[0].transport, cJSON_GetObjectItem(ip1, "transport")->valuestring);
        }
        cJSON *ip2 = cJSON_GetObjectItem(ip_prof, "ip2");
        if (ip2) {
            global_ip_profiles[1].client = cJSON_GetObjectItem(ip2, "client")->valueint;
            strcpy(global_ip_profiles[1].address, cJSON_GetObjectItem(ip2, "address")->valuestring);
            global_ip_profiles[1].port = cJSON_GetObjectItem(ip2, "port")->valueint;
            strcpy(global_ip_profiles[1].transport, cJSON_GetObjectItem(ip2, "transport")->valuestring);
        }
    }

    cJSON *mqtt_prof = cJSON_GetObjectItem(root, "mqtt_profile");
    if (mqtt_prof) {
        cJSON *mq1 = cJSON_GetObjectItem(mqtt_prof, "mqtt1");
        if (mq1) {
            strcpy(global_mqtt_profiles[0].tx_topic, cJSON_GetObjectItem(mq1, "tx_topic")->valuestring);
            strcpy(global_mqtt_profiles[0].rx_topic, cJSON_GetObjectItem(mq1, "rx_topic")->valuestring);
        }
        cJSON *mq2 = cJSON_GetObjectItem(mqtt_prof, "mqtt2");
        if (mq2) {
            strcpy(global_mqtt_profiles[1].tx_topic, cJSON_GetObjectItem(mq2, "tx_topic")->valuestring);
            strcpy(global_mqtt_profiles[1].rx_topic, cJSON_GetObjectItem(mq2, "rx_topic")->valuestring);
        }
    }

    cJSON *routing = cJSON_GetObjectItem(root, "routing");
    if (routing) {
        cJSON *gw = cJSON_GetObjectItem(routing, "gateway");
        if (gw) {
            cJSON *u1 = cJSON_GetObjectItem(gw, "uart1");
            if (u1) {
                strcpy(global_routing_config.gateway[0].intf, cJSON_GetObjectItem(u1, "intf")->valuestring);
                cJSON *p = cJSON_GetObjectItem(u1, "profile");
                strcpy(global_routing_config.gateway[0].profile, p ? p->valuestring : "");
            }
            cJSON *u2 = cJSON_GetObjectItem(gw, "uart2");
            if (u2) {
                strcpy(global_routing_config.gateway[1].intf, cJSON_GetObjectItem(u2, "intf")->valuestring);
                cJSON *p = cJSON_GetObjectItem(u2, "profile");
                strcpy(global_routing_config.gateway[1].profile, p ? p->valuestring : "");
            }
        }
        cJSON *mon = cJSON_GetObjectItem(routing, "monitor");
        if (mon) {
            cJSON *u1 = cJSON_GetObjectItem(mon, "uart1");
            if (u1) {
                strcpy(global_routing_config.monitor[0].intf, cJSON_GetObjectItem(u1, "intf")->valuestring);
                cJSON *p = cJSON_GetObjectItem(u1, "profile");
                strcpy(global_routing_config.monitor[0].profile, p ? p->valuestring : "");
            }
            cJSON *u2 = cJSON_GetObjectItem(mon, "uart2");
            if (u2) {
                strcpy(global_routing_config.monitor[1].intf, cJSON_GetObjectItem(u2, "intf")->valuestring);
                cJSON *p = cJSON_GetObjectItem(u2, "profile");
                strcpy(global_routing_config.monitor[1].profile, p ? p->valuestring : "");
            }
        }
    }

    cJSON_Delete(root);
    free(data);
}

