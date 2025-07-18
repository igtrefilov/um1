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
#include <esp_wifi.h>
#include <esp_http_server.h>

#include <nvs_flash.h>
#include <sys/param.h>
#include "cJSON.h"

#include "protocol_examples_utils.h"

#include "um1_lan.h"

esp_err_t handle_get_config(httpd_req_t *req);
esp_err_t config_save_handler(httpd_req_t *req);
esp_err_t file_upload_handler(httpd_req_t *req);
esp_err_t ota_update_handler(httpd_req_t *req);
esp_err_t spiffs_get_handler(httpd_req_t *req);
esp_err_t echo_handler(httpd_req_t *req);
esp_err_t reboot_handler(httpd_req_t *req);
httpd_handle_t start_webserver(void);

#endif // UM1_LAN_H
