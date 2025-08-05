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

#include "um1_auth.h"
#include "um1_uart.h"
#include "um1_lan.h"
#include "um1_wifi.h"
#include "um1_http_server.h"
#include "um1_spiffs.h"
#include "um1_config.h"
#include "um1_mqtt.h"
#include "um1_sntp.h"

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
    init_esp_environment();

    start_spiffs();
    vTaskDelay(pdMS_TO_TICKS(1000));
	read_config_and_apply();

	start_wifi();
	vTaskDelay(pdMS_TO_TICKS(1000));

	start_lan();
	vTaskDelay(pdMS_TO_TICKS(1000));

	start_webserver();
	vTaskDelay(pdMS_TO_TICKS(1000));

	start_uart();
	vTaskDelay(pdMS_TO_TICKS(1000));

	start_mqtt();
	vTaskDelay(pdMS_TO_TICKS(1000));

	start_sntp();
	vTaskDelay(pdMS_TO_TICKS(1000));

	token_auth_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
