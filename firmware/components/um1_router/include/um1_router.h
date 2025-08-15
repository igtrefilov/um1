#ifndef UM1_ROUTER_H
#define UM1_ROUTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "esp_netif.h"

#include "um1_config.h"
#include "um1_uart.h"
#include "um1_mqtt.h"

#ifdef __cplusplus
extern "C" {
#endif

void router_start(void);
void router_on_uart_rx(int uart_port, const uint8_t *data, size_t len);
void router_on_mqtt_message(const char *topic, const uint8_t *data, size_t len);

esp_netif_t *router_get_netif_lan(void);
esp_netif_t *router_get_netif_ap(void);
esp_netif_t *router_get_netif_sta(void);

void router_set_netif_lan(esp_netif_t *n);
void router_set_netif_ap(esp_netif_t *n);
void router_set_netif_sta(esp_netif_t *n);

#ifdef __cplusplus
}
#endif

#endif // UM1_ROUTER_H
