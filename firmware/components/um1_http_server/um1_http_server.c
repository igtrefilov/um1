#include "um1_http_server.h"

#define MAX_SUBSCRIBERS 10
#define UPLOAD_BUFFER_SIZE  1024

httpd_handle_t global_http_server = NULL;
static ws_subscriber_t subscribers[MAX_SUBSCRIBERS];
static size_t subs_count = 0;
static int ota_ws_fd = -1;

static const char *TAG = "http_server";

httpd_uri_t login = {
    .uri      = "/login",
    .method   = HTTP_POST,
    .handler  = handle_login,
    .user_ctx = NULL
};

httpd_uri_t logout = {
    .uri      = "/logout",
    .method   = HTTP_POST,
    .handler  = handle_logout,
    .user_ctx = NULL
};

httpd_uri_t change_password = {
    .uri      = "/api/change_password",
    .method   = HTTP_POST,
    .handler  = handle_change_password,
    .user_ctx = NULL
};

httpd_uri_t system_info = {
    .uri       = "/api/system_info",
    .method    = HTTP_GET,
    .handler   = system_info_handler,
    .user_ctx  = NULL
};

httpd_uri_t config_get_uri = {
    .uri      = "/api/config",
    .method   = HTTP_GET,
    .handler  = handle_get_config,
    .user_ctx = NULL
};

httpd_uri_t stream_status = {
    .uri      = "/api/stream_status",
    .method   = HTTP_GET,
    .handler  = stream_status_handler,
    .user_ctx = NULL
};

const httpd_uri_t config_save = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_save_handler,
    .user_ctx  = NULL
};

const httpd_uri_t upload = {
    .uri       = "/upload",
    .method    = HTTP_POST,
    .handler   = file_upload_handler,
    .user_ctx  = NULL
};

const httpd_uri_t ota_update = {
    .uri       = "/ota",
    .method    = HTTP_POST,
    .handler   = ota_update_handler,
    .user_ctx  = NULL
};

const httpd_uri_t spiffs_uri = {
    .uri       = "/*",
    .method    = HTTP_GET,
    .handler   = spiffs_get_handler,
    .user_ctx  = NULL
};

const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_control_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

const httpd_uri_t reboot = {
    .uri       = "/reboot",
    .method    = HTTP_GET,
    .handler   = reboot_handler,
    .user_ctx  = NULL
};

static void add_subscriber(int fd, bool uart1, bool uart2) {
    if (subs_count < MAX_SUBSCRIBERS) {
        subscribers[subs_count++] = (ws_subscriber_t){ .fd = fd, .uart1 = uart1, .uart2 = uart2 };
        ESP_LOGI(TAG, "Subscriber added: fd=%d, uart1=%d, uart2=%d, total=%d", fd, uart1, uart2, subs_count);
    }
}

static void remove_subscriber(int fd) {
    for (size_t i = 0; i < subs_count; ++i) {
        if (subscribers[i].fd == fd) {
            subscribers[i] = subscribers[--subs_count];
            ESP_LOGI(TAG, "Subscriber removed, total = %d", subs_count);
            break;
        }
    }
}

esp_err_t system_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "cpu_freq_mhz", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    uint32_t flash_size = 0;
    esp_flash_get_size(esp_flash_default_chip, &flash_size);
    cJSON_AddNumberToObject(root, "flash_size_bytes", flash_size);

    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "internal_free_heap", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(root, "dram_free_heap", heap_caps_get_free_size(MALLOC_CAP_DMA));

    uint64_t uptime_ms = esp_timer_get_time() / 1000;
    cJSON_AddNumberToObject(root, "uptime_ms", uptime_ms);

    cJSON_AddStringToObject(root, "sdk_version", esp_get_idf_version());

    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        cJSON_AddStringToObject(root, "running_partition", running->label);
    }

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    cJSON_AddNumberToObject(root, "revision", chip_info.revision);
    cJSON_AddNumberToObject(root, "features", chip_info.features);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac_address", mac_str);

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddNumberToObject(root, "wifi_rssi", ap_info.rssi);
    }

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (it) {
        cJSON *partitions = cJSON_CreateArray();
        do {
            const esp_partition_t *p = esp_partition_get(it);
            if (p) {
                cJSON *entry = cJSON_CreateObject();
                cJSON_AddStringToObject(entry, "label", p->label);
                cJSON_AddNumberToObject(entry, "address", p->address);
                cJSON_AddNumberToObject(entry, "size", p->size);
                cJSON_AddItemToArray(partitions, entry);
            }
        } while ((it = esp_partition_next(it)) != NULL);
        cJSON_AddItemToObject(root, "app_partitions", partitions);
        esp_partition_iterator_release(it);
    }

    char *out = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);

    free(out);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t stream_status_handler(httpd_req_t *req)
{
	bool tcp_connected = 0; //temporary stub
	bool udp_connected = 0; //temporary stub
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "tcp", tcp_connected);
    cJSON_AddBoolToObject(root, "udp", udp_connected);
    char *out = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    free(out);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t handle_get_config(httpd_req_t *req)
{
    FILE *fp = fopen("/spiffs/src/config.json", "r");
    if (!fp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);

    char *buf = malloc(file_size + 1);
    if (!buf) {
        fclose(fp);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    fread(buf, 1, file_size, fp);
    buf[file_size] = '\0';
    fclose(fp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, file_size);
    free(buf);
    return ESP_OK;
}

esp_err_t config_save_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    int cur_len = 0, received;
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[cur_len] = '\0';


    FILE *f = fopen("/spiffs/src/config.json", "r");
    char *file_buf = NULL;
    cJSON *root = NULL;

    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        file_buf = malloc(fsize + 1);
        if (file_buf) {
            fread(file_buf, 1, fsize, f);
            file_buf[fsize] = '\0';
            root = cJSON_Parse(file_buf);
            free(file_buf);
        }
        fclose(f);
    }

    if (!root) root = cJSON_CreateObject();

    cJSON *incoming = cJSON_Parse(buf);
    free(buf);

    if (!incoming) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, incoming) {
    	cJSON *item = NULL;
    	cJSON_ArrayForEach(item, incoming) {
    	    cJSON_DeleteItemFromObject(root, item->string);
    	    cJSON_AddItemToObject(root, item->string, cJSON_Duplicate(item, 1));
    	}
    }

    char *out = cJSON_Print(root);
    cJSON_Delete(incoming);
    cJSON_Delete(root);

    f = fopen("/spiffs/src/config.json", "w");
    if (!f) {
        free(out);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    fwrite(out, 1, strlen(out), f);
    fclose(f);
    free(out);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t file_upload_handler(httpd_req_t *req)
{
    char filename[64];
    FILE *f = NULL;
    char buf[UPLOAD_BUFFER_SIZE];
    int remaining = req->content_len;
    int r;

    char fname_header[56] = {0};
    size_t hl = httpd_req_get_hdr_value_len(req, "X-FILENAME");
    if (hl > 0 && hl < sizeof(fname_header)) {
        httpd_req_get_hdr_value_str(req, "X-FILENAME", fname_header, hl + 1);
        ESP_LOGI(TAG, "fname_header: %s", fname_header);

        snprintf(filename, sizeof(filename), "/spiffs/%s", fname_header);

    } else {
        strcpy(filename, "/spiffs/upload.bin");
    }
    ESP_LOGI(TAG, "Filename: %s", filename);

    f = fopen(filename, "wb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file for writing");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int to_read = MIN(remaining, UPLOAD_BUFFER_SIZE);
        printf("remainning: %d\n", remaining);
        r = httpd_req_recv(req, buf, to_read);
        if (r <= 0) {
            fclose(f);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file data");
            return ESP_FAIL;
        }
        fwrite(buf, 1, r, f);
        remaining -= r;
    }
    fclose(f);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t ota_update_handler(httpd_req_t *req)
{
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition not found");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[UPLOAD_BUFFER_SIZE];
    int remaining = req->content_len;
    int total = req->content_len;
    int written = 0;

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Transfer-Encoding", "chunked");

    while (remaining > 0) {
        int to_read = MIN(remaining, sizeof(buf));
        int r = httpd_req_recv(req, buf, to_read);
        if (r <= 0) {
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA receive error");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, r);
        if (err != ESP_OK) {
            esp_ota_end(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write error");
            return ESP_FAIL;
        }

        written += r;
        remaining -= r;

        int percent = (written * 100) / total;

        if (ota_ws_fd != -1) {
            char ws_msg[32];
            snprintf(ws_msg, sizeof(ws_msg), "progress:%d", percent);
            printf("WS send progress: %s\n", ws_msg);
            httpd_ws_frame_t ws_pkt = {
                .payload = (uint8_t *)ws_msg,
                .len = strlen(ws_msg),
                .type = HTTPD_WS_TYPE_TEXT
            };

            esp_err_t ws_err = httpd_ws_send_frame_async(req->handle, ota_ws_fd, &ws_pkt);
            if (ws_err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send OTA progress over WebSocket: %s", esp_err_to_name(ws_err));
                ota_ws_fd = -1;
            }
        }
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end error");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    if (ota_ws_fd != -1) {
        const char *done_msg = "done";
        httpd_ws_frame_t ws_pkt = {
            .payload = (uint8_t *)done_msg,
            .len = strlen(done_msg),
            .type = HTTPD_WS_TYPE_TEXT
        };
        httpd_ws_send_frame_async(req->handle, ota_ws_fd, &ws_pkt);
        ota_ws_fd = -1;
    }

    httpd_resp_send_chunk(req, "done\n", strlen("done\n"));
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(TAG, "OTA update complete, restarting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

esp_err_t spiffs_get_handler(httpd_req_t *req)
{
    char path[512];
    const char *base_path = "/spiffs/src";
    strlcpy(path, base_path, sizeof(path));

    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(path, req->uri, sizeof(path));
        strlcat(path, "index.html", sizeof(path));
    } else {
        strlcat(path, req->uri, sizeof(path));
    }

    if (!is_public_uri(req->uri) && !token_is_valid(req)) {
        ESP_LOGW(TAG, "Unauthorized access to %s", req->uri);
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login.html");
        httpd_resp_sendstr(req, "Redirecting...");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Open path: %s", path);
    FILE *file = fopen(path, "r");
    if (!file) {
        ESP_LOGE(TAG, "File not found: %s", path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    const char *ext = strrchr(path, '.');
    if (ext) {
        if      (strcmp(ext, ".html") == 0) httpd_resp_set_type(req, "text/html");
        else if (strcmp(ext, ".css")  == 0) httpd_resp_set_type(req, "text/css");
        else if (strcmp(ext, ".js")   == 0) httpd_resp_set_type(req, "application/javascript");
        else if (strcmp(ext, ".png")  == 0) httpd_resp_set_type(req, "image/png");
        else if (strcmp(ext, ".jpg")  == 0) httpd_resp_set_type(req, "image/jpeg");
        else                                httpd_resp_set_type(req, "application/octet-stream");
    }

    char chunk[128];
    size_t len;
    while ((len = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, len) != ESP_OK) {
            fclose(file);
            ESP_LOGE(TAG, "Error sending chunk");
            return ESP_FAIL;
        }
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t ws_control_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK || ws_pkt.len == 0) return ret;

    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) { free(buf); return ret; }

    ESP_LOGI(TAG, "Received message: %s", buf);
    int fd = httpd_req_to_sockfd(req);

    if (strcmp((char*)buf, "STOP_STREAM") == 0) {
        remove_subscriber(fd);
        free(buf);
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse((char*)buf);
    if (root) {
        const cJSON *action = cJSON_GetObjectItem(root, "action");
        if (action && strcmp(action->valuestring, "START_STREAM") == 0) {
            bool uart1 = cJSON_IsTrue(cJSON_GetObjectItem(root, "uart1"));
            bool uart2 = cJSON_IsTrue(cJSON_GetObjectItem(root, "uart2"));
            add_subscriber(fd, uart1, uart2);
            cJSON_Delete(root);
            free(buf);
            return ESP_OK;
        }
        cJSON_Delete(root);
    }

    if (strcmp((char*)buf, "OTA_PROGRESS") == 0) {
        ota_ws_fd = fd;
        free(buf);
        return ESP_OK;
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    free(buf);
    return ret;
}

void send_uart_ws_data(int uart_port, const uint8_t *data, size_t len) {
    char msg[1024];

    for (size_t i = 0; i < subs_count; ++i) {
        bool enabled = (uart_port == UART_PORT_NUM_1) ? subscribers[i].uart1 : subscribers[i].uart2;
        if (!enabled) continue;

        int offset = 0;

        if (is_sntp_enabled() && len >= sizeof(uint64_t)) {
            uint64_t ts_us;
            memcpy(&ts_us, data, sizeof(uint64_t));
            ts_us = reverse_bytes_u64(ts_us);

            time_t ts_sec = ts_us / 1000000;
            int ts_usec = ts_us % 1000000;

            struct tm timeinfo;
            localtime_r(&ts_sec, &timeinfo);

            offset += snprintf(msg + offset, sizeof(msg) - offset,
                               "%04d-%02d-%02d %02d:%02d:%02d.%06d ",
                               timeinfo.tm_year + 1900,
                               timeinfo.tm_mon + 1,
                               timeinfo.tm_mday,
                               timeinfo.tm_hour,
                               timeinfo.tm_min,
                               timeinfo.tm_sec,
                               ts_usec);

            data += sizeof(uint64_t);
            len  -= sizeof(uint64_t);
        }

        const char *prefix = uart_port == UART_PORT_NUM_1 ? "uart1:" : "uart2:";
        offset += snprintf(msg + offset, sizeof(msg) - offset, "%s", prefix);

        for (int j = 0; j < len && offset < sizeof(msg) - 3; ++j) {
            offset += snprintf(msg + offset, sizeof(msg) - offset, "%02X", data[j]);
        }

        httpd_ws_frame_t ws_pkt = {
            .payload = (uint8_t *)msg,
            .len = strlen(msg),
            .type = HTTPD_WS_TYPE_TEXT
        };

        extern httpd_handle_t global_http_server;
        if (global_http_server) {
            httpd_ws_send_frame_async(global_http_server, subscribers[i].fd, &ws_pkt);
        }
    }
}

esp_err_t reboot_handler(httpd_req_t *req)
{
    const char *resp = "MCU is rebooting...\n";
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(100));

    esp_restart();

    return ESP_OK;
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

esp_err_t stop_webserver(httpd_handle_t server)
{
    return httpd_stop(server);
}

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &login);
        httpd_register_uri_handler(server, &logout);
        httpd_register_uri_handler(server, &change_password);
        httpd_register_uri_handler(server, &system_info);
        httpd_register_uri_handler(server, &config_get_uri);
        httpd_register_uri_handler(server, &config_save);
        httpd_register_uri_handler(server, &stream_status);
        httpd_register_uri_handler(server, &upload);
        httpd_register_uri_handler(server, &ota_update);
        httpd_register_uri_handler(server, &reboot);

        httpd_register_uri_handler(server, &spiffs_uri);

        global_http_server = server;

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

uint64_t reverse_bytes_u64(uint64_t value)
{
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) << 8)  |
           ((value & 0x000000FF00000000ULL) >> 8)  |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
}
