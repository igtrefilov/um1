#include "um1_uart.h"

typedef struct {
    QueueHandle_t rx_to_net_queue;
    QueueHandle_t net_to_tx_queue;
} uart_task_ctx_t;

static uart_task_ctx_t uart_ctx;

void uart_task_rx(void *arg) {
    /*uart_task_ctx_t *ctx = (uart_task_ctx_t *)arg;
    uint8_t byte;
    while (1) {
        if (uart_read_bytes(UART_NUM_1, &byte, 1, portMAX_DELAY) > 0) {
            xQueueSend(ctx->rx_to_net_queue, &byte, portMAX_DELAY);
        }
    }*/
	while(1){
		printf("uart_task_rx \n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void uart_task_tx(void *arg) {
    /*uart_task_ctx_t *ctx = (uart_task_ctx_t *)arg;
    uint8_t byte;
    while (1) {
        if (xQueueReceive(ctx->net_to_tx_queue, &byte, portMAX_DELAY)) {
            uart_write_bytes(UART_NUM_1, (char *)&byte, 1);
        }
    }*/
	while(1){
		printf("uart_task_tx \n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void start_uart_tasks(QueueHandle_t rx_to_net, QueueHandle_t net_to_tx)
{
    uart_ctx.rx_to_net_queue = rx_to_net;
    uart_ctx.net_to_tx_queue = net_to_tx;

    xTaskCreate(uart_task_rx, "uart_rx", 4096, &uart_ctx, 3, NULL);
    xTaskCreate(uart_task_tx, "uart_tx", 4096, &uart_ctx, 3, NULL);
}
