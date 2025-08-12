#ifndef UM1_LAN_H
#define UM1_LAN_H

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stddef.h"
#include <stdbool.h>
#include "esp_netif.h"
#include "esp_log.h"

#include "start_lan.h"
#include "um1_spiffs.h"


void send_tcp_packet(const uint8_t *data, size_t len);
void send_udp_packet(const uint8_t *data, size_t len);
bool lan_tcp_connected(void);
bool lan_udp_connected(void);

void lan_tcp_task(void *arg);
void lan_udp_task(void *arg);

void extern_util_task(void *pvParameters);
void handle_util(int client_sock);

#endif // UM1_LAN_H
