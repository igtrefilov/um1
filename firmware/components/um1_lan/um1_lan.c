#include "um1_lan.h"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <strings.h>
#include "um1_uart.h"

#define TCP_PORT  3333
#define LISTEN_BACKLOG 5

#define CMD_BUF_SZ   64
#define CHUNK_SZ     1024


static const char *TAG = "eth_if_tasks";

static int tcp_sock = -1;
static int tcp_listen_sock = -1;
static int udp_sock = -1;
static struct sockaddr_in tcp_dest;
static struct sockaddr_in udp_dest;
bool tcp_connected = false;
bool udp_connected = false;

static void process_stream_buffer(const char *buf, int len);
static void tcp_client_task(void *arg);
static void tcp_server_task(void *arg);
static void udp_client_task(void *arg);
static void udp_server_task(void *arg);

static void send_text(int sock, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len > 0) {
        send(sock, buf, len, 0);
    }
}

static void tree_dir(const char *path, int sock, int level) {
    DIR *d = opendir(path);
    if (!d) {
        send_text(sock, "ERR: cannot open %s\n", path);
        return;
    }
    struct dirent *entry;
    char fullpath[256];
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        for (int i = 0; i < level; i++) send(sock, "  ", 2, 0);
        send_text(sock, "%s\n", entry->d_name);
        strlcpy(fullpath, path, sizeof(fullpath));
        strlcat(fullpath, "/", sizeof(fullpath));
        strlcat(fullpath, entry->d_name, sizeof(fullpath));
        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            tree_dir(fullpath, sock, level + 1);
        }
    }
    closedir(d);
}

static void rmdir_recursive(const char *base_path, int sock) {
    DIR *d = opendir(base_path);
    if (!d) {
        send_text(sock, "ERR: cannot open %s\n", base_path);
        return;
    }
    struct dirent *entry;
    char fullpath[1024];
    struct stat st;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;
        snprintf(fullpath, sizeof(fullpath), "%s/%s", base_path, entry->d_name);
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            rmdir_recursive(fullpath, sock);
        } else {
            unlink(fullpath);
        }
    }
    closedir(d);
    rmdir(base_path);
}

static void process_stream_buffer(const char *buf, int len) {
    if (len <= 0) return;
    if (strncmp(buf, "uart1:", 6) == 0) {
        uart_write_bytes(UART_PORT_NUM_1, buf + 6, len - 6);
    } else if (strncmp(buf, "uart2:", 6) == 0) {
        uart_write_bytes(UART_PORT_NUM_2, buf + 6, len - 6);
    }
}

void init_stream_sockets(void) {
    if (global_tcp_config.enabled) {
        if (strcmp(global_tcp_config.role, "server") == 0) {
            tcp_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (tcp_listen_sock >= 0) {
                struct sockaddr_in addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(global_tcp_config.port),
                    .sin_addr.s_addr = htonl(INADDR_ANY)
                };
                if (bind(tcp_listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
                    listen(tcp_listen_sock, 1) == 0) {
                    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(tcp_listen_sock);
                    tcp_listen_sock = -1;
                }
            }
        } else {
            tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (tcp_sock >= 0) {
                tcp_dest.sin_family = AF_INET;
                tcp_dest.sin_port = htons(global_tcp_config.port);
                inet_pton(AF_INET, global_tcp_config.server, &tcp_dest.sin_addr);
                if (connect(tcp_sock, (struct sockaddr *)&tcp_dest, sizeof(tcp_dest)) == 0) {
                    tcp_connected = true;
                    xTaskCreate(tcp_client_task, "tcp_client_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(tcp_sock);
                    tcp_sock = -1;
                }
            }
        }
    }

    if (global_udp_config.enabled) {
        udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sock >= 0) {
            if (strcmp(global_udp_config.role, "server") == 0) {
                struct sockaddr_in addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(global_udp_config.port),
                    .sin_addr.s_addr = htonl(INADDR_ANY)
                };
                if (bind(udp_sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                    udp_connected = true;
                    xTaskCreate(udp_server_task, "udp_server_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(udp_sock);
                    udp_sock = -1;
                }
            } else {
                udp_dest.sin_family = AF_INET;
                udp_dest.sin_port = htons(global_udp_config.port);
                inet_pton(AF_INET, global_udp_config.server, &udp_dest.sin_addr);
                if (connect(udp_sock, (struct sockaddr *)&udp_dest, sizeof(udp_dest)) == 0) {
                    udp_connected = true;
                    xTaskCreate(udp_client_task, "udp_client_task", 4096, NULL, tskIDLE_PRIORITY + 5, NULL);
                } else {
                    close(udp_sock);
                    udp_sock = -1;
                }
            }
        }
    }
}

void send_tcp_packet(int uart_port, const uint8_t *data, size_t len) {
    if (!tcp_connected) return;

    char buffer[BUF_SIZE + 16];
    int offset = 0;
    if (uart_port >= 0) {
        offset = snprintf(buffer, sizeof(buffer), "uart%d:", uart_port);
    }
    if (offset + len > sizeof(buffer)) len = sizeof(buffer) - offset;
    memcpy(buffer + offset, data, len);
    if (send(tcp_sock, buffer, offset + len, 0) < 0) {
        close(tcp_sock);
        tcp_sock = -1;
        tcp_connected = false;
    }
}

void send_udp_packet(int uart_port, const uint8_t *data, size_t len) {
    if (!udp_connected) return;

    char buffer[BUF_SIZE + 16];
    int offset = 0;
    if (uart_port >= 0) {
        offset = snprintf(buffer, sizeof(buffer), "uart%d:", uart_port);
    }
    if (offset + len > sizeof(buffer)) len = sizeof(buffer) - offset;
    memcpy(buffer + offset, data, len);
    if (send(udp_sock, buffer, offset + len, 0) < 0) {
        close(udp_sock);
        udp_sock = -1;
        udp_connected = false;
    }
}

static void tcp_client_task(void *arg) {
    char buffer[BUF_SIZE];
    while (1) {
        int len = recv(tcp_sock, buffer, sizeof(buffer), 0);
        if (len <= 0) break;
        route_data("LAN", (uint8_t *)buffer, len);
        route_data("AP", (uint8_t *)buffer, len);
        process_stream_buffer(buffer, len);
    }
    close(tcp_sock);
    tcp_sock = -1;
    tcp_connected = false;
    vTaskDelete(NULL);
}

static void tcp_server_task(void *arg) {
    char buffer[BUF_SIZE];
    while (1) {
        int client = accept(tcp_listen_sock, NULL, NULL);
        if (client < 0) continue;
        tcp_sock = client;
        tcp_connected = true;
        while (1) {
            int len = recv(client, buffer, sizeof(buffer), 0);
            if (len <= 0) break;
            route_data("LAN", (uint8_t *)buffer, len);
            route_data("AP", (uint8_t *)buffer, len);
            process_stream_buffer(buffer, len);
        }
        close(client);
        tcp_sock = -1;
        tcp_connected = false;
    }
}

static void udp_client_task(void *arg) {
    char buffer[BUF_SIZE];
    while (1) {
        int len = recv(udp_sock, buffer, sizeof(buffer), 0);
        if (len <= 0) break;
        route_data("LAN", (uint8_t *)buffer, len);
        route_data("AP", (uint8_t *)buffer, len);
        process_stream_buffer(buffer, len);
    }
    close(udp_sock);
    udp_sock = -1;
    udp_connected = false;
    vTaskDelete(NULL);
}

static void udp_server_task(void *arg) {
    char buffer[BUF_SIZE];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    while (1) {
        int len = recvfrom(udp_sock, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&src_addr, &addrlen);
        if (len < 0) break;
        route_data("LAN", (uint8_t *)buffer, len);
        route_data("AP", (uint8_t *)buffer, len);
        process_stream_buffer(buffer, len);
    }
    close(udp_sock);
    udp_sock = -1;
    udp_connected = false;
    vTaskDelete(NULL);
}

void handle_client(int client_sock) {
    char cmd[CMD_BUF_SZ];
    int len = recv(client_sock, cmd, sizeof(cmd) - 1, 0);
    if (len <= 0) {
        close(client_sock);
        return;
    }
    cmd[len] = '\0';
    ESP_LOGI(TAG, "Received cmd: '%s'", cmd);
    route_data("LAN", (uint8_t *)cmd, len);

    if (strncmp(cmd, "TREE ", 5) == 0) {
        char dir[128];
        if (sscanf(cmd + 5, "%127s", dir) == 1) {
            tree_dir(dir, client_sock, 0);
            send_text(client_sock, "END\n");
        }

    } else if (strncmp(cmd, "LIST ", 5) == 0) {
        char dir[128];
        if (sscanf(cmd + 5, "%127s", dir) == 1) {
            DIR *d = opendir(dir);
            if (!d) {
                send_text(client_sock, "ERR: cannot open %s\n", dir);
            } else {
                struct dirent *e;
                while ((e = readdir(d)) != NULL) {
                    if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
                        send_text(client_sock, "%s\n", e->d_name);
                }
                closedir(d);
                send_text(client_sock, "END\n");
            }
        }

    } else if (strncmp(cmd, "MKDIR ", 6) == 0) {
        char path[128];
        if (sscanf(cmd + 6, "%127s", path) == 1) {
            if (mkdir(path, 0755) == 0) send_text(client_sock, "OK\n");
            else send_text(client_sock, "ERR\n");
        }

    }	else if (strncmp(cmd, "RMDIR ", 6) == 0) {
			char path[128];
			if (sscanf(cmd + 6, "%127s", path) == 1) {
				ESP_LOGI(TAG, "RMDIR %s", path);
				extern void rmdir_recursive(const char *path, int sock);
				rmdir_recursive(path, client_sock);
				send_text(client_sock, "OK\n");
			}


    } else if (strncmp(cmd, "DEL ", 4) == 0) {
        char path[128];
        if (sscanf(cmd + 4, "%127s", path) == 1) {
            if (unlink(path) == 0) send_text(client_sock, "OK\n");
            else send_text(client_sock, "ERR\n");
        }

    } else if (strncmp(cmd, "STORE ", 6) == 0) {
        char fname[64];
        long filesize;
        if (sscanf(cmd + 6, "%63s %ld", fname, &filesize) == 2) {
            ESP_LOGI(TAG, "STORE %s (%ld bytes)", fname, filesize);
            send_text(client_sock, "OK\n");
            // receive and write
            FILE *f = fopen(fname, "wb");
            if (!f) { send_text(client_sock, "ERR\n"); close(client_sock); return; }
            long rem = filesize;
            uint8_t buf[CHUNK_SZ];
            while (rem > 0) {
                int to_read = rem > CHUNK_SZ ? CHUNK_SZ : rem;
                int r = recv(client_sock, buf, to_read, 0);
                if (r <= 0) break;
                route_data("LAN", buf, r);
                fwrite(buf, 1, r, f);
                rem -= r;
            }
            fclose(f);
            send_text(client_sock, rem == 0 ? "DONE\n" : "FAIL\n");
        }

    } else if (strncmp(cmd, "GET ", 4) == 0) {
        char path[128];
        if (sscanf(cmd + 4, "%127s", path) == 1) {
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
                send_text(client_sock, "ERR: no such file\n");
            } else {
                FILE *f = fopen(path, "rb");
                if (!f) {
                    send_text(client_sock, "ERR: cannot open\n");
                } else {
                    long size = st.st_size;
                    send_text(client_sock, "SIZE %ld\n", size);
                    uint8_t buf[CHUNK_SZ];
                    long sent = 0;
                    while (sent < size) {
                        int to_read = (size - sent) > CHUNK_SZ ? CHUNK_SZ : (size - sent);
                        int r = fread(buf, 1, to_read, f);
                        if (r <= 0) break;
                        send(client_sock, buf, r, 0);
                        route_data("LAN", buf, r);
                        sent += r;
                    }
                    fclose(f);
                }
            }
        }

	}else if(strncmp(cmd, "FORMAT", 6) == 0){

		esp_vfs_spiffs_unregister(NULL);
		esp_err_t err = esp_spiffs_format(NULL);
		start_spiffs();

		if (err == ESP_OK) {
			send_text(client_sock, "OK\n");
		} else {
			send_text(client_sock, "ERR: %s\n", esp_err_to_name(err));
		}

		} else {
			send_text(client_sock, "UNKNOWN CMD\n");
    }
    close(client_sock);
}

void handle_client_task(void *pvParameters) {
    int client_sock = *((int *)pvParameters);
    free(pvParameters);

    handle_client(client_sock);

    close(client_sock);
    vTaskDelete(NULL);
}

void tcp_lan_server_task(void *pvParameters) {
    esp_netif_ip_info_t *ip_info = (esp_netif_ip_info_t *)pvParameters;
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        free(ip_info);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = ip_info->ip.addr,
        .sin_port = htons(TCP_PORT),
    };
    free(ip_info);

    int err = bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    err = listen(listen_sock, LISTEN_BACKLOG);
    if (err < 0) {
        ESP_LOGE(TAG, "Error during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t socklen = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &socklen);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        ESP_LOGI(TAG, "Accepted connection from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        int *pclient = malloc(sizeof(int));
        if (pclient == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for client socket");
            close(client_sock);
            continue;
        }
        *pclient = client_sock;
        xTaskCreate(handle_client_task, "handle_client_task", 4096, pclient, 5, NULL);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

void udp_lan_server_task(void *pvParameters) {
    esp_netif_ip_info_t *ip_info = (esp_netif_ip_info_t *)pvParameters;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket: errno %d", errno);
        free(ip_info);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT),
        .sin_addr.s_addr = ip_info->ip.addr,
    };

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "UDP socket bind failed: errno %d", errno);
        close(sock);
        free(ip_info);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP server listening on port %d", TCP_PORT);

    char rx_buffer[1024];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        rx_buffer[len] = 0;
        ESP_LOGI(TAG, "Received UDP: '%s' from %s:%d", rx_buffer,
                 inet_ntoa(source_addr.sin_addr), ntohs(source_addr.sin_port));

        sendto(sock, rx_buffer, len, 0,
               (struct sockaddr *)&source_addr, sizeof(source_addr));
    }

    close(sock);
    vTaskDelete(NULL);
}
