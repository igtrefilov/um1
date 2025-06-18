#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stddef.h"
#include "start_lan.h"

void lan_task_rx(void *arg);
void lan_task_tx(void *arg);
void start_lan_tasks(QueueHandle_t rx_to_net, QueueHandle_t net_to_tx);
