#include "um1_http_server.h"

#define MAX_SUBSCRIBERS 10
#define UPLOAD_BUFFER_SIZE  1024

static int subscribers[MAX_SUBSCRIBERS];
static size_t subs_count = 0;

static const char *TAG = "http_server";

httpd_uri_t config_get_uri = {
    .uri      = "/api/config",
    .method   = HTTP_GET,
    .handler  = handle_get_config,
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
        .handler    = echo_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

const httpd_uri_t reboot = {
    .uri       = "/reboot",
    .method    = HTTP_GET,
    .handler   = reboot_handler,
    .user_ctx  = NULL
};

static void add_subscriber(int fd) {
	if (subs_count < MAX_SUBSCRIBERS) {
		subscribers[subs_count++] = fd;
		ESP_LOGI(TAG, "Subscriber added, total = %d", subs_count);
	}
}

static void remove_subscriber(int fd) {
	for (size_t i = 0; i < subs_count; ++i) {
		if (subscribers[i] == fd) {
			subscribers[i] = subscribers[--subs_count];
			ESP_LOGI(TAG, "Subscriber removed, total = %d", subs_count);
			break;
		}
	}
}

static void stream_task(void *arg) {
    httpd_handle_t hd = (httpd_handle_t)arg;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        static int counter = 0;
        char data[64];
        int len = snprintf(data, sizeof(data), "Stream data #%d", counter++);
        httpd_ws_frame_t ws_pkt = {
            .payload = (uint8_t*)data,
            .len     = len,
            .type    = HTTPD_WS_TYPE_TEXT
        };
        for (size_t i = 0; i < subs_count; ++i) {
            httpd_ws_send_frame_async(hd, subscribers[i], &ws_pkt);
        }
    }
    vTaskDelete(NULL);
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
    char buf[1024];
    int total_len = req->content_len;
    int cur_len = 0, received;

    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, sizeof(buf) - cur_len);
        if (received <= 0) return ESP_FAIL;
        cur_len += received;
    }

    buf[cur_len] = '\0';

    FILE *f = fopen("/spiffs/src/config.json", "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open config.json for writing");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    fwrite(buf, 1, cur_len, f);
    fclose(f);

    ESP_LOGI(TAG, "Config saved: %.*s", cur_len, buf);

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
        char progress[64];
        int len = snprintf(progress, sizeof(progress), "progress:%d\n", percent);
        httpd_resp_send_chunk(req, progress, len);

        printf("OTA progress: %d%% (%d/%d)\n", percent, written, total);
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
    strlcat(path, req->uri, sizeof(path));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(path, "index.html", sizeof(path));
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
        else                                 httpd_resp_set_type(req, "application/octet-stream");
    }

    char chunk[128];
    size_t len;
    while ((len = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        httpd_resp_send_chunk(req, chunk, len);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t echo_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;
    if (ws_pkt.len == 0) return ESP_OK;

    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) { free(buf); return ret; }

    ESP_LOGI(TAG, "Received message: %s", buf);
    int fd = httpd_req_to_sockfd(req);

    if (strcmp((char*)buf, "START_STREAM") == 0) {
        add_subscriber(fd);
        free(buf);
        return ESP_OK;
    } else if (strcmp((char*)buf, "STOP_STREAM") == 0) {
        remove_subscriber(fd);
        free(buf);
        return ESP_OK;
    }
    ret = httpd_ws_send_frame(req, &ws_pkt);
    free(buf);
    return ret;
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

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}


httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &config_get_uri);
        httpd_register_uri_handler(server, &config_save);
        httpd_register_uri_handler(server, &upload);
        httpd_register_uri_handler(server, &ota_update);
        httpd_register_uri_handler(server, &reboot);

        httpd_register_uri_handler(server, &spiffs_uri);


        xTaskCreate(stream_task, "ws_stream_task", 4096, server, tskIDLE_PRIORITY + 1, NULL);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}
