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
#include "esp_netif.h"
#include "esp_log.h"

#include "start_lan.h"
#include "um1_spiffs.h"

void handle_client(int client_sock);
void tcp_lan_server_task(void *pvParameters);
void udp_lan_server_task(void *pvParameters);

#endif // UM1_LAN_H
