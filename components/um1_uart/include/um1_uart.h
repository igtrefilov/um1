#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stddef.h"

void uart_task_rx(void *arg);
void uart_task_tx(void *arg);
void start_uart_tasks(QueueHandle_t rx_to_net, QueueHandle_t net_to_tx);
