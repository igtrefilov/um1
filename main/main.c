#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "um1_uart.h"
#include "um1_lan.h"
#include "um1_wifi.h"
#include "um1_http_server.h"

void init_esp_environment(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void app_main(void)
{
    //QueueHandle_t uart_to_net = xQueueCreate(512, sizeof(uint8_t));
    //QueueHandle_t net_to_uart = xQueueCreate(512, sizeof(uint8_t));

    init_esp_environment();

	start_softap();
	vTaskDelay(pdMS_TO_TICKS(3000));
	start_lan();
	vTaskDelay(pdMS_TO_TICKS(3000));
	start_webserver();

    //start_uart_tasks(uart_to_net, net_to_uart);
    //start_lan_tasks(net_to_uart, uart_to_net);
    // start_wifi_tasks(...);
    // start_bt_tasks(...);
    // start_ble_tasks(...);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
