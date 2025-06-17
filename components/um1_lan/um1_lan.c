#include "um1_lan.h"

typedef struct {
    QueueHandle_t rx_to_net_queue;
    QueueHandle_t net_to_tx_queue;
} lan_task_ctx_t;

static lan_task_ctx_t lan_ctx;

void lan_task_rx(void *arg) {
    /*lan_task_ctx_t *ctx = (lan_task_ctx_t *)arg;
    uint8_t byte;
    while (1) {
        if (lan_receive(&byte, 1)) {
            xQueueSend(ctx->rx_to_net_queue, &byte, portMAX_DELAY);
        }
    }*/
	while(1){
		printf("lan_task_rx \n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void lan_task_tx(void *arg) {
    /*lan_task_ctx_t *ctx = (lan_task_ctx_t *)arg;
    uint8_t byte;
    while (1) {
        if (xQueueReceive(ctx->net_to_tx_queue, &byte, portMAX_DELAY)) {
            lan_send(&byte, 1);
        }
    }*/
	while(1){
		printf("lan_task_tx \n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void start_lan_tasks(QueueHandle_t rx_to_net, QueueHandle_t net_to_tx)
{
    lan_ctx.rx_to_net_queue = rx_to_net;
    lan_ctx.net_to_tx_queue = net_to_tx;

    xTaskCreate(lan_task_rx, "lan_rx", 4096, &lan_ctx, 2, NULL);
    xTaskCreate(lan_task_tx, "lan_tx", 4096, &lan_ctx, 2, NULL);
}

