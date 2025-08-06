#ifndef UM1_HTTP_SERVER_H
#define UM1_HTTP_SERVER_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>

#include <esp_log.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include <esp_system.h>
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_vfs.h"
#include "esp_ota_ops.h"

#include "esp_cpu.h"
#include "esp_flash.h"
#include "esp_chip_info.h"

#include "esp_mac.h"

#include "esp_timer.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "spi_flash_mmap.h"

#include <esp_wifi.h>
#include <esp_http_server.h>

#include <nvs_flash.h>
#include <sys/param.h>
#include "cJSON.h"

#include "protocol_examples_utils.h"

#include "um1_lan.h"
#include "um1_uart.h"
#include "um1_sntp.h"
#include "um1_auth.h"

typedef struct {
    int fd;
    bool uart1;
    bool uart2;
} ws_subscriber_t;

esp_err_t system_info_handler(httpd_req_t *req);
esp_err_t handle_get_config(httpd_req_t *req);
esp_err_t config_save_handler(httpd_req_t *req);
esp_err_t file_upload_handler(httpd_req_t *req);
esp_err_t ota_update_handler(httpd_req_t *req);
esp_err_t spiffs_get_handler(httpd_req_t *req);
esp_err_t ws_control_handler(httpd_req_t *req);
void send_uart_ws_data(int uart_port, const uint8_t *data, size_t len);
esp_err_t reboot_handler(httpd_req_t *req);
esp_err_t stream_status_handler(httpd_req_t *req);
httpd_handle_t start_webserver(void);

#endif // UM1_LAN_H
