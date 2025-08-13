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

void lan_tcp_server_task(void *pvParameters);
void lan_udp_server_task(void *pvParameters);
void util_server_task(void *pvParameters);

#endif // UM1_LAN_H
