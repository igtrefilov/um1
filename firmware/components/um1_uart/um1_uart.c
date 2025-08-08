#include "um1_uart.h"
#include "um1_router.h"
#include <string.h>
#include <strings.h>

/*static const char *TAG = "um1_uart";*/


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


uint64_t reverse_bytes_u64(uint64_t value) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 8;
        result |= (value & 0xFF);
        value >>= 8;
    }
    return result;
}
