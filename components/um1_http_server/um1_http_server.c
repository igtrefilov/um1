#include "um1_http_server.h"

#define MAX_SUBSCRIBERS 10
#define UPLOAD_BUFFER_SIZE  1024

static int subscribers[MAX_SUBSCRIBERS];
static size_t subs_count = 0;

static const char *TAG = "http_server";

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

static esp_err_t upload_post_handler(httpd_req_t *req)
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

static esp_err_t spiffs_get_handler(httpd_req_t *req)
{
    char path[512];
    const char *base_path = "/spiffs";

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

static esp_err_t echo_handler(httpd_req_t *req) {
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

static const httpd_uri_t upload = {
    .uri       = "/upload",
    .method    = HTTP_POST,
    .handler   = upload_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t spiffs_uri = {
    .uri       = "/*",
    .method    = HTTP_GET,
    .handler   = spiffs_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = echo_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

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
    // Stop the httpd server
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
        httpd_register_uri_handler(server, &upload);
        httpd_register_uri_handler(server, &spiffs_uri);


        xTaskCreate(stream_task, "ws_stream_task", 4096, server, tskIDLE_PRIORITY + 1, NULL);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


/*
#include "um1_http_server.h"

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)
#define UPLOAD_BUFFER_SIZE  1024

static const char *TAG = "http_server";

static esp_err_t spiffs_get_handler(httpd_req_t *req)
{
    char path[512];
    const char *base_path = "/spiffs";

    // Копируем базовый путь
    strlcpy(path, base_path, sizeof(path));
    // Добавляем URI
    strlcat(path, req->uri, sizeof(path));
    // Если URI оканчивается '/', то добавляем index.html
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(path, "index.html", sizeof(path));
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        ESP_LOGE(TAG, "File not found: %s", path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    // Определяем MIME‑тип по расширению
    const char *ext = strrchr(path, '.');
    if (ext) {
        if      (strcmp(ext, ".html") == 0) httpd_resp_set_type(req, "text/html");
        else if (strcmp(ext, ".css")  == 0) httpd_resp_set_type(req, "text/css");
        else if (strcmp(ext, ".js")   == 0) httpd_resp_set_type(req, "application/javascript");
        else if (strcmp(ext, ".png")  == 0) httpd_resp_set_type(req, "image/png");
        else if (strcmp(ext, ".jpg")  == 0) httpd_resp_set_type(req, "image/jpeg");
        else                                 httpd_resp_set_type(req, "application/octet-stream");
    }

    // Шлём файл чанками
    char chunk[128];
    size_t len;
    while ((len = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        httpd_resp_send_chunk(req, chunk, len);
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

 An HTTP GET handler
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

     Get header value string length and allocate memory for length + 1,
     * extra byte for null termination
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
         Copy null terminated value string into buffer
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

     Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN], dec_param[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
             Get value of expected key from query string
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
                example_uri_decode(dec_param, param, strnlen(param, EXAMPLE_HTTP_QUERY_KEY_MAX_LEN));
                ESP_LOGI(TAG, "Decoded query parameter => %s", dec_param);
            }
        }
        free(buf);
    }

     Set some custom headers
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

     Send response with custom headers and body set as the
     * string passed in user context
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

     After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now.
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
     Let's pass response string in user
     * context to demonstrate it's usage
    .user_ctx  = "<b1 style='color: green;'>Hello World, Man!</b1>"
};

 An HTTP POST handler
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
         Read the data for the request
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                 Retry receiving if timeout occurred
                continue;
            }
            return ESP_FAIL;
        }

         Send back the same data
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

         Log data received
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
         Return ESP_OK to keep underlying socket open
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
         Return ESP_FAIL to close underlying socket
        return ESP_FAIL;
    }
     For any other URI send 404 and close socket
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

 An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers

static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
         URI handlers can be unregistered using the uri string
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
         Register the custom error handler
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
         Unregister custom error handler
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

     Respond with empty body
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filename[64];
    FILE *f = NULL;
    char buf[UPLOAD_BUFFER_SIZE];
    int remaining = req->content_len;
    int r;

    // Получим имя файла из заголовка
    char fname_header[56] = {0};
    size_t hl = httpd_req_get_hdr_value_len(req, "X-FILENAME");
    if (hl > 0 && hl < sizeof(fname_header)) {
        httpd_req_get_hdr_value_str(req, "X-FILENAME", fname_header, hl + 1);
        ESP_LOGI(TAG, "fname_header: %s", fname_header);

        // Безопасно формируем путь к файлу
        snprintf(filename, sizeof(filename), "/spiffs/%s", fname_header);

    } else {
        // если не передали — сохраняем под фиксированным именем
        strcpy(filename, "/spiffs/upload.bin");
    }
    ESP_LOGI(TAG, "Filename: %s", filename);

    // Открываем файл на запись (перезапись если есть)
    f = fopen(filename, "wb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file for writing");
        return ESP_FAIL;
    }

    // Читаем тело запроса и пишем в файл
    while (remaining > 0) {
        int to_read = MIN(remaining, UPLOAD_BUFFER_SIZE);
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

    // Ответ клиенту
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t upload = {
    .uri       = "/upload",
    .method    = HTTP_POST,
    .handler   = upload_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t spiffs_uri = {
    .uri       = "/",               // ловим все GET‑запросы
    .method    = HTTP_GET,
    .handler   = spiffs_get_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &upload);
        httpd_register_uri_handler(server, &spiffs_uri);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

void disconnect_handler(void* arg, esp_event_base_t event_base,
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

void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}
*/
