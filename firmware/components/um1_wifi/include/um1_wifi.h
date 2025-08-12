#ifndef UM1_WIFI_H
#define UM1_WIFI_H

#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "um1_config.h"

extern int wifi_ap_tcp_server_sock;
extern int wifi_ap_tcp_client_sock;
extern int wifi_ap_udp_server_sock;
extern int wifi_ap_udp_client_sock;

void start_wifi(void);

#endif // UM1_WIFI_H
