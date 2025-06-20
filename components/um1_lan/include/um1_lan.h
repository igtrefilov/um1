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
void tcp_server_task(void *pvParameters);
void lan_task_rx(void *arg);
void lan_task_tx(void *arg);
void start_lan_tasks(QueueHandle_t rx_to_net, QueueHandle_t net_to_tx);
