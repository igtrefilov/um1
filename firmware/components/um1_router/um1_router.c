#include "um1_router.h"
#include "um1_config.h"
#include "um1_mqtt.h"
#include "um1_lan.h"
#include <driver/uart.h>
#include <strings.h>

#define UART_PORT_NUM_1    1
#define UART_PORT_NUM_2    2

void route_data(const char *src_if, const uint8_t *data, size_t len) {
    int src_uart_port = -1;
    if (strcasecmp(src_if, "UART1") == 0) src_uart_port = UART_PORT_NUM_1;
    else if (strcasecmp(src_if, "UART2") == 0) src_uart_port = UART_PORT_NUM_2;

    for (int i = 0; i < global_route_count; ++i) {
        route_config_t *r = &global_routes[i];
        if (!r->active) continue;
        if (strcasecmp(r->src.interface, src_if) != 0) continue;

        if (strcasecmp(r->dst.interface, "LAN") == 0 ||
            strcasecmp(r->dst.interface, "AP") == 0 ||
            strcasecmp(r->dst.interface, "STA") == 0) {
            if (global_tcp_config.enabled) {
                send_tcp_packet(src_uart_port, data, len);
            }
            if (global_udp_config.enabled) {
                send_udp_packet(src_uart_port, data, len);
            }
        } else if (strcasecmp(r->dst.interface, "MQTT") == 0 || strcasecmp(r->dst.protocol, "MQTT") == 0) {
            esp_mqtt_client_handle_t client = get_mqtt_client_handle();
            if (client != NULL && global_mqtt_config.enabled && global_mqtt_config.tx_enabled) {
                const char *topic;
                if (r->dst.topic[0]) {
                    topic = r->dst.topic;
                } else if (src_uart_port == UART_PORT_NUM_1) {
                    topic = "uart/1";
                } else if (src_uart_port == UART_PORT_NUM_2) {
                    topic = "uart/2";
                } else {
                    topic = "data";
                }
                esp_mqtt_client_publish(client, topic, (const char *)data, len, 1, 0);
            }
        } else if (strcasecmp(r->dst.interface, "UART1") == 0 || strcasecmp(r->dst.interface, "UART2") == 0) {
            int dst_port = (strcasecmp(r->dst.interface, "UART1") == 0) ? UART_PORT_NUM_1 : UART_PORT_NUM_2;
            uart_write_bytes(dst_port, (const char *)data, len);
        }
    }
}
