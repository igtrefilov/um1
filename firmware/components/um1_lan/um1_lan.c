#include "um1_lan.h"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <strings.h>
#include <driver/uart.h>
#include <stdarg.h>
#include "um1_router.h"

#include "freertos/semphr.h"

#define TCP_DATA_PORT    4001
#define TCP_UTILITY_PORT 3333
#define LISTEN_BACKLOG   5

#define CMD_BUF_SZ   64
#define CHUNK_SZ     1024

#define MAX_TCP_CLIENTS 4

static const char *TAG = "um1_lan";

/* ------- shared socket state ------- */

static int s_tcp_clients[MAX_TCP_CLIENTS];
static int s_udp_sock = -1;
static struct sockaddr_in s_udp_peer;
static bool s_udp_peer_set = false;

static SemaphoreHandle_t s_sock_lock = NULL;

static inline void lock_socks(void)   { if (s_sock_lock) xSemaphoreTake(s_sock_lock, portMAX_DELAY); }
static inline void unlock_socks(void) { if (s_sock_lock) xSemaphoreGive(s_sock_lock); }

static void sockets_state_init_once(void)
{
    static bool inited = false;
    if (!inited) {
        s_sock_lock = xSemaphoreCreateMutex();
        for (int i = 0; i < MAX_TCP_CLIENTS; ++i) s_tcp_clients[i] = -1;
        s_udp_sock = -1;
        s_udp_peer_set = false;
        inited = true;
    }
}

static void clients_add(int sock)
{
    lock_socks();
    for (int i = 0; i < MAX_TCP_CLIENTS; ++i) {
        if (s_tcp_clients[i] == -1) { s_tcp_clients[i] = sock; break; }
    }
    unlock_socks();
}

static void clients_remove(int sock)
{
    lock_socks();
    for (int i = 0; i < MAX_TCP_CLIENTS; ++i) {
        if (s_tcp_clients[i] == sock) { s_tcp_clients[i] = -1; break; }
    }
    unlock_socks();
}

/* ------- outward API ------- */

void send_tcp_packet(const uint8_t *data, size_t len)
{
    if (!data || !len) return;

    lock_socks();
    for (int i = 0; i < MAX_TCP_CLIENTS; ++i) {
        int s = s_tcp_clients[i];
        if (s < 0) continue;
        int r = send(s, data, len, 0);
        if (r < 0) {
            ESP_LOGE(TAG, "send(TCP fd=%d) failed, errno=%d — drop client", s, errno);
            shutdown(s, SHUT_RDWR);
            close(s);
            s_tcp_clients[i] = -1;
        }
    }
    unlock_socks();
}

void send_udp_packet(const uint8_t *data, size_t len)
{
    if (!data || !len) return;

    int sock;
    bool has_peer;
    struct sockaddr_in peer;

    lock_socks();
    sock = s_udp_sock;
    has_peer = s_udp_peer_set;
    peer = s_udp_peer;
    unlock_socks();

    if (sock < 0) {
        ESP_LOGW(TAG, "UDP socket not ready");
        return;
    }
    if (!has_peer) {
        ESP_LOGW(TAG, "UDP peer not set yet (no incoming datagrams received)");
        return;
    }

    int r = sendto(sock, data, len, 0, (struct sockaddr *)&peer, sizeof(peer));
    if (r < 0) {
        ESP_LOGE(TAG, "sendto failed: errno=%d", errno);
    }
}

/* ------- TCP data server ------- */

static void handle_lan_tcp_client_task(void *pvParameters)
{
    int sock = *((int *)pvParameters);
    free(pvParameters);

    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    getpeername(sock, (struct sockaddr *)&peer, &plen);
    uint32_t ip = peer.sin_addr.s_addr;
    uint16_t port = ntohs(peer.sin_port);

    uint8_t buf[CHUNK_SZ];
    while (1) {
        int r = recv(sock, buf, sizeof(buf), 0);
        if (r > 0) {
            rx_meta_t m = {
                .src_id = IF_LAN,
                .proto = PROTO_TCP,
                .ip = ip,
                .port = port,
                .topic = NULL
            };
            router_route(&m, buf, r);
            continue;
        }
        if (r == 0) {
            ESP_LOGI(TAG, "TCP client closed");
            break;
        }
        ESP_LOGE(TAG, "recv error: errno=%d", errno);
        break;
    }

    clients_remove(sock);
    shutdown(sock, SHUT_RDWR);
    close(sock);
    vTaskDelete(NULL);
}

void lan_tcp_task(void *arg)
{
    sockets_state_init_once();

    esp_netif_ip_info_t ip_local = *(esp_netif_ip_info_t *)arg;

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "lan_tcp: socket() failed: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(TCP_DATA_PORT),
        .sin_addr.s_addr = ip_local.ip.addr,
    };

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "lan_tcp: bind() failed: errno=%d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, LISTEN_BACKLOG) < 0) {
        ESP_LOGE(TAG, "lan_tcp: listen() failed: errno=%d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "LAN TCP server listening on %s:%d",
             inet_ntoa(addr.sin_addr), TCP_DATA_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "lan_tcp: accept() failed: errno=%d", errno);
            continue;
        }

        ESP_LOGI(TAG, "LAN TCP client %s:%d connected",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        clients_add(client_sock);

        int *pclient = malloc(sizeof(int));
        if (!pclient) {
            ESP_LOGE(TAG, "lan_tcp: malloc() failed for client socket");
            shutdown(client_sock, SHUT_RDWR);
            close(client_sock);
            clients_remove(client_sock);
            continue;
        }
        *pclient = client_sock;
        xTaskCreate(handle_lan_tcp_client_task, "lan_tcp_client", 4096, pclient, 5, NULL);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

/* ------- UDP data server ------- */

void lan_udp_task(void *arg)
{
    sockets_state_init_once();

    esp_netif_ip_info_t ip_local = *(esp_netif_ip_info_t *)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "lan_udp: socket() failed: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(TCP_DATA_PORT),
        .sin_addr.s_addr = ip_local.ip.addr,
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "lan_udp: bind() failed: errno=%d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    lock_socks();
    s_udp_sock = sock;
    unlock_socks();

    ESP_LOGI(TAG, "LAN UDP server listening on %s:%d",
             inet_ntoa(addr.sin_addr), TCP_DATA_PORT);

    uint8_t buf[CHUNK_SZ];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        int r = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr *)&from, &fromlen);
        if (r < 0) {
            ESP_LOGE(TAG, "lan_udp: recvfrom() error: errno=%d", errno);
            continue;
        }

        lock_socks();
        s_udp_peer = from;
        s_udp_peer_set = true;
        unlock_socks();

        rx_meta_t m = {
            .src_id = IF_LAN,
            .proto = PROTO_UDP,
            .ip = from.sin_addr.s_addr,
            .port = ntohs(from.sin_port),
            .topic = NULL
        };
        router_route(&m, buf, r);

    }

    close(sock);
    vTaskDelete(NULL);
}

/*	EXTERN UTILITY	*/

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

void handle_util(int client_sock) {
    char cmd[CMD_BUF_SZ];
    int len = recv(client_sock, cmd, sizeof(cmd) - 1, 0);
    if (len <= 0) {
        close(client_sock);
        return;
    }
    cmd[len] = '\0';
    ESP_LOGI(TAG, "Received cmd: '%s'", cmd);

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

void handle_util_task(void *pvParameters) {
    int client_sock = *((int *)pvParameters);
    free(pvParameters);

    handle_util(client_sock);

    close(client_sock);
    vTaskDelete(NULL);
}

void extern_util_task(void *pvParameters) {
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
        .sin_port = htons(TCP_UTILITY_PORT),
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

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_UTILITY_PORT);

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
        xTaskCreate(handle_util_task, "handle_client_task", 4096, pclient, 5, NULL);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}
