#include "um1_lan.h"

#define TCP_PORT  3333
#define LISTEN_BACKLOG 5

static const char *TAG = "um1_lan";

typedef struct {
    esp_netif_t *netif;
    uint16_t port;
} tcp_srv_args_t;

static void tcp_server_task_entry(void *pv) {
    tcp_srv_args_t *a = (tcp_srv_args_t *)pv;
    run_tcp_server(a->netif, a->port);
    free(a);
    vTaskDelete(NULL);
}

void lan_tcp_server_task(void *pvParameters) {
    esp_netif_t *netif = (esp_netif_t *) pvParameters;
    ESP_LOGI(TAG, "LAN TCP master task start (spawning per-port servers)");

    tcp_srv_args_t *a1 = malloc(sizeof(*a1));
    tcp_srv_args_t *a2 = malloc(sizeof(*a2));
    if (!a1 || !a2) {
        free(a1); free(a2);
        ESP_LOGE(TAG, "malloc failed for tcp server args");
        vTaskDelete(NULL);
        return;
    }

    *a1 = (tcp_srv_args_t){ .netif = netif, .port = LAN_TCP_PORT };
    *a2 = (tcp_srv_args_t){ .netif = netif, .port = EXTERN_UTILITY_PORT };

    xTaskCreate(tcp_server_task_entry, "tcp_srv_main",    4096, a1, tskIDLE_PRIORITY + 5, NULL);
    xTaskCreate(tcp_server_task_entry, "tcp_srv_utility", 4096, a2, tskIDLE_PRIORITY + 5, NULL);

    vTaskDelete(NULL);
}

void lan_udp_server_task(void *pvParameters) {
    esp_netif_t *netif = (esp_netif_t *)pvParameters;
    ESP_LOGI(TAG, "LAN UDP server task start");
    run_udp_server(netif, LAN_UDP_PORT);
    vTaskDelete(NULL);
}
