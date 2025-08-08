#include "um1_uart.h"
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <strings.h>

/*static const char *TAG = "um1_uart";*/

static int tcp_sock = -1;
static int tcp_listen_sock = -1;
static int udp_sock = -1;
static struct sockaddr_in tcp_dest;
static struct sockaddr_in udp_dest;
bool tcp_connected = false;
bool udp_connected = false;

static void process_stream_buffer(const char *buf, int len);
static void route_data(const char *src_if, const uint8_t *data, size_t len);
static void tcp_client_task(void *arg);
static void tcp_server_task(void *arg);
static void udp_client_task(void *arg);
static void udp_server_task(void *arg);

void start_uart(void){
	um1_uart_config_t uart1_cfg = global_uart_config[0];
	um1_uart_config_t uart2_cfg = global_uart_config[1];

	uart_config_t uart1_config = {
		.baud_rate = uart1_cfg.baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = strcmp(uart1_cfg.parity, "even") == 0 ? UART_PARITY_EVEN :
				  strcmp(uart1_cfg.parity, "odd") == 0 ? UART_PARITY_ODD : UART_PARITY_DISABLE,
		.stop_bits = uart1_cfg.stop_bits == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT
	};

	uart_config_t uart2_config = {
		.baud_rate = uart2_cfg.baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = strcmp(uart2_cfg.parity, "even") == 0 ? UART_PARITY_EVEN :
				  strcmp(uart2_cfg.parity, "odd") == 0 ? UART_PARITY_ODD : UART_PARITY_DISABLE,
		.stop_bits = uart2_cfg.stop_bits == 2 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT
	};

	int intr_alloc_flags = 0;

	ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM_1, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
	ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM_2, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));

	ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM_1, &uart1_config));
	ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM_2, &uart2_config));

	ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM_1, UART1_TXD, UART1_RXD, -1, -1));
	ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM_2, UART2_TXD, UART2_RXD, -1, -1));

	xTaskCreate(&uart1_task, "uart1_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
	xTaskCreate(&uart2_task, "uart2_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
}

void uart1_task(void *arg) {
    uint8_t *uart_buffer = (uint8_t *)malloc(BUF_SIZE);
    if (uart_buffer == NULL) {
        printf("Failed to allocate memory for uart_buffer\n");
        vTaskDelete(NULL);
    }

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM_1, uart_buffer, BUF_SIZE - 1, 1 / portTICK_PERIOD_MS);
        if (len > 0) {
        	send_uart_packet_with_timestamp(UART_PORT_NUM_1, uart_buffer, len);
        }
        vTaskDelay(1);
    }

    free(uart_buffer);
    vTaskDelete(NULL);
}

void uart2_task(void *arg) {
    uint8_t *uart_buffer = (uint8_t *)malloc(BUF_SIZE);
    if (uart_buffer == NULL) {
        printf("Failed to allocate memory for uart_buffer\n");
        vTaskDelete(NULL);
    }

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM_2, uart_buffer, BUF_SIZE - 1, 1 / portTICK_PERIOD_MS);
        if (len > 0) {
        	send_uart_packet_with_timestamp(UART_PORT_NUM_2, uart_buffer, len);
        }
        vTaskDelay(1);
    }

    free(uart_buffer);
    vTaskDelete(NULL);
}

void send_uart_packet_with_timestamp(int uart_port, const uint8_t *data, size_t len) {
    uint8_t extended_buffer[BUF_SIZE + sizeof(uint64_t)];
    size_t offset = 0;

    if (is_sntp_enabled()) {
        uint64_t ts = get_ntp_time_us();
        ts = reverse_bytes_u64(ts);
        memcpy(extended_buffer, &ts, sizeof(uint64_t));
        offset = sizeof(uint64_t);
    }

    if (offset + len > sizeof(extended_buffer)) {
        len = sizeof(extended_buffer) - offset;
    }

    memcpy(extended_buffer + offset, data, len);
    size_t total_len = offset + len;

    // WebSocket
    send_uart_ws_data(uart_port, extended_buffer, total_len);

    // Route to other interfaces according to routing table
    route_data(uart_port == UART_PORT_NUM_1 ? "UART1" : "UART2",
               extended_buffer, total_len);
}

void init_stream_sockets(void) {
    if (global_tcp_config.enabled) {
        if (strcmp(global_tcp_config.role, "server") == 0) {
            tcp_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (tcp_listen_sock >= 0) {
                struct sockaddr_in addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(global_tcp_config.port),
                    .sin_addr.s_addr = htonl(INADDR_ANY)
                };
                if (bind(tcp_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
                    listen(tcp_listen_sock, 1) == 0) {
                    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(tcp_listen_sock);
                    tcp_listen_sock = -1;
                }
            }
        } else {
            tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (tcp_sock >= 0) {
                tcp_dest.sin_family = AF_INET;
                tcp_dest.sin_port = htons(global_tcp_config.port);
                inet_pton(AF_INET, global_tcp_config.server, &tcp_dest.sin_addr);
                if (connect(tcp_sock, (struct sockaddr *)&tcp_dest, sizeof(tcp_dest)) == 0) {
                    tcp_connected = true;
                    xTaskCreate(tcp_client_task, "tcp_client_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(tcp_sock);
                    tcp_sock = -1;
                }
            }
        }
    }

    if (global_udp_config.enabled) {
        udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sock >= 0) {
            if (strcmp(global_udp_config.role, "server") == 0) {
                struct sockaddr_in addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(global_udp_config.port),
                    .sin_addr.s_addr = htonl(INADDR_ANY)
                };
                if (bind(udp_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                    udp_connected = true;
                    xTaskCreate(udp_server_task, "udp_server_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(udp_sock);
                    udp_sock = -1;
                }
            } else {
                udp_dest.sin_family = AF_INET;
                udp_dest.sin_port = htons(global_udp_config.port);
                inet_pton(AF_INET, global_udp_config.server, &udp_dest.sin_addr);
                if (connect(udp_sock, (struct sockaddr *)&udp_dest, sizeof(udp_dest)) == 0) {
                    udp_connected = true;
                    xTaskCreate(udp_client_task, "udp_client_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(udp_sock);
                    udp_sock = -1;
                }
            }
        }
    }
}

void send_tcp_packet(int uart_port, const uint8_t *data, size_t len) {
    if (!tcp_connected) return;

    char buffer[BUF_SIZE + 16];
    int offset = 0;
    if (uart_port >= 0) {
        offset = snprintf(buffer, sizeof(buffer), "uart%d:", uart_port);
    }
    if (offset + len > sizeof(buffer)) len = sizeof(buffer) - offset;
    memcpy(buffer + offset, data, len);
    if (send(tcp_sock, buffer, offset + len, 0) < 0) {
        close(tcp_sock);
        tcp_sock = -1;
        tcp_connected = false;
    }
}

void send_udp_packet(int uart_port, const uint8_t *data, size_t len) {
    if (!udp_connected) return;

    char buffer[BUF_SIZE + 16];
    int offset = 0;
    if (uart_port >= 0) {
        offset = snprintf(buffer, sizeof(buffer), "uart%d:", uart_port);
    }
    if (offset + len > sizeof(buffer)) len = sizeof(buffer) - offset;
    memcpy(buffer + offset, data, len);
    if (send(udp_sock, buffer, offset + len, 0) < 0) {
        close(udp_sock);
        udp_sock = -1;
        udp_connected = false;
    }
}

static void route_data(const char *src_if, const uint8_t *data, size_t len) {
    int src_uart_port = -1;
    if (strcasecmp(src_if, "UART1") == 0) src_uart_port = UART_PORT_NUM_1;
    else if (strcasecmp(src_if, "UART2") == 0) src_uart_port = UART_PORT_NUM_2;

    for (int i = 0; i < global_route_count; ++i) {
        route_config_t *r = &global_routes[i];
        if (!r->active) continue;
        if (strcasecmp(r->src.interface, src_if) != 0) continue;

        if (strcasecmp(r->dst.interface, "LAN") == 0) {
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

static void process_stream_buffer(const char *buf, int len) {
    if (len <= 0) return;
    if (strncmp(buf, "uart1:", 6) == 0) {
        uart_write_bytes(UART_PORT_NUM_1, buf + 6, len - 6);
    } else if (strncmp(buf, "uart2:", 6) == 0) {
        uart_write_bytes(UART_PORT_NUM_2, buf + 6, len - 6);
    }
}

static void tcp_client_task(void *arg) {
    char buffer[BUF_SIZE];
    while (1) {
        int len = recv(tcp_sock, buffer, sizeof(buffer), 0);
        if (len <= 0) break;
        route_data("LAN", (uint8_t *)buffer, len);
        route_data("AP", (uint8_t *)buffer, len);
        process_stream_buffer(buffer, len);
    }
    close(tcp_sock);
    tcp_sock = -1;
    tcp_connected = false;
    vTaskDelete(NULL);
}

static void tcp_server_task(void *arg) {
    char buffer[BUF_SIZE];
    while (1) {
        int client = accept(tcp_listen_sock, NULL, NULL);
        if (client < 0) continue;
        tcp_sock = client;
        tcp_connected = true;
        while (1) {
            int len = recv(client, buffer, sizeof(buffer), 0);
            if (len <= 0) break;
            route_data("LAN", (uint8_t *)buffer, len);
            route_data("AP", (uint8_t *)buffer, len);
            process_stream_buffer(buffer, len);
        }
        close(client);
        tcp_sock = -1;
        tcp_connected = false;
    }
}

static void udp_client_task(void *arg) {
    char buffer[BUF_SIZE];
    while (1) {
        int len = recv(udp_sock, buffer, sizeof(buffer), 0);
        if (len <= 0) break;
        route_data("LAN", (uint8_t *)buffer, len);
        route_data("AP", (uint8_t *)buffer, len);
        process_stream_buffer(buffer, len);
    }
    close(udp_sock);
    udp_sock = -1;
    udp_connected = false;
    vTaskDelete(NULL);
}

static void udp_server_task(void *arg) {
    char buffer[BUF_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    while (1) {
        int len = recvfrom(udp_sock, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&src_addr, &addrlen);
        if (len < 0) break;
        route_data("LAN", (uint8_t *)buffer, len);
        route_data("AP", (uint8_t *)buffer, len);
        process_stream_buffer(buffer, len);
    }
    close(udp_sock);
    udp_sock = -1;
    udp_connected = false;
    vTaskDelete(NULL);
}

uint64_t reverse_bytes_u64(uint64_t value) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 8;
        result |= (value & 0xFF);
        value >>= 8;
    }
    return result;
}
