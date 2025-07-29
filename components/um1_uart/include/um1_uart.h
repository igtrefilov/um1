#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stddef.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "um1_http_server.h"
#include "um1_config.h"

#define UART1_TXD 2
#define UART1_RXD 4

#define UART2_TXD 13
#define UART2_RXD 35

#define UART_PORT_NUM_1    1
#define UART_PORT_NUM_2    2
#define UART_BAUD_RATE     9600
#define UART_TASK_STACK_SIZE    2048
#define BUF_SIZE (1024)

void start_uart(void);
void uart1_task(void *arg);
void uart2_task(void *arg);

