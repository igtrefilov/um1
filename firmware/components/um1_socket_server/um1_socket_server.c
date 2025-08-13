#include "um1_socket_server.h"

static const char *TAG = "um1_socket_server";

bool fill_sockaddr_from_netif(esp_netif_t *netif, uint16_t port, struct sockaddr_in *addr_out) {
    memset(addr_out, 0, sizeof(*addr_out));
    addr_out->sin_family = AF_INET;
    addr_out->sin_port = htons(port);

    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
            addr_out->sin_addr.s_addr = ip.ip.addr;
            return true;
        }
    }
    addr_out->sin_addr.s_addr = htonl(INADDR_ANY);
    return true;
}

void run_tcp_server(esp_netif_t *netif, uint16_t port) {
    for (;;) {
        int listen_fd = -1, client_fd = -1;
        struct sockaddr_in addr;
        if (!fill_sockaddr_from_netif(netif, port, &addr)) {
            ESP_LOGE(TAG, "TCP: cannot build sockaddr");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (listen_fd < 0) {
            ESP_LOGE(TAG, "TCP: socket() failed: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int yes = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "TCP: bind() failed on %s:%d err=%d",
                     inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), errno);
            close(listen_fd);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (listen(listen_fd, LISTEN_BACKLOG) < 0) {
            ESP_LOGE(TAG, "TCP: listen() failed: %d", errno);
            close(listen_fd);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "TCP: listening on %s:%d",
                 inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        for (;;) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd < 0) {
                ESP_LOGW(TAG, "TCP: accept() err=%d; restarting listen", errno);
                break;
            }

            ESP_LOGI(TAG, "TCP: client connected %s:%d",
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            if (port == EXTERN_UTILITY_PORT) {
                ESP_LOGI(TAG, "Extern utility connection on port %d", EXTERN_UTILITY_PORT);
                int *pfd = malloc(sizeof(int));
                if (!pfd) {
                    ESP_LOGE(TAG, "malloc failed for client fd");
                    close(client_fd);
                    client_fd = -1;
                    continue;
                }
                *pfd = client_fd;
                if (xTaskCreate(util_client_task, "util_client_task", 4096, pfd, 5, NULL) != pdPASS) {
                    ESP_LOGE(TAG, "xTaskCreate(util_client_task) failed");
                    free(pfd);
                    close(client_fd);
                }
                client_fd = -1;
                continue;
            }

            struct timeval tv = {.tv_sec = 60, .tv_usec = 0};
            setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            uint8_t buf[1024];
            for (;;) {
                ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
                if (n > 0) {
                    buf[n] = 0;
                    ESP_LOGI(TAG, "TCP RECV (%zd bytes): %s", n, (char *)buf);
                } else if (n == 0) {
                    ESP_LOGI(TAG, "TCP: client closed connection");
                    break;
                } else {
                    if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
                    ESP_LOGW(TAG, "TCP: recv() err=%d; closing client", errno);
                    break;
                }
            }

            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
            client_fd = -1;
            ESP_LOGI(TAG, "TCP: client session ended");
        }

        if (client_fd >= 0) {
            shutdown(client_fd, SHUT_RDWR);
            close(client_fd);
        }
        if (listen_fd >= 0) close(listen_fd);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void run_udp_server(esp_netif_t *netif, uint16_t port) {
    for (;;) {
        int sock = -1;
        struct sockaddr_in addr;
        if (!fill_sockaddr_from_netif(netif, port, &addr)) {
            ESP_LOGE(TAG, "UDP: cannot build sockaddr");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "UDP: socket() failed: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int yes = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "UDP: bind() failed on %s:%d err=%d",
                     inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "UDP: listening on %s:%d",
                 inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        for (;;) {
            uint8_t buf[1500];
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                 (struct sockaddr *)&from, &fromlen);
            if (n > 0) {
                buf[n] = 0;
                ESP_LOGI(TAG, "UDP RECV %s:%d (%zd bytes): %s",
                         inet_ntoa(from.sin_addr), ntohs(from.sin_port), n, (char *)buf);
            } else if (n < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
                ESP_LOGW(TAG, "UDP: recvfrom() err=%d; restarting socket", errno);
                break;
            }
        }

        if (sock >= 0) close(sock);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

