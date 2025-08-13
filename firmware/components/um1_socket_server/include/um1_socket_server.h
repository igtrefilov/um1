#ifndef UM1_SOCKET_SERVER_H
#define UM1_SOCKET_SERVER_H

#include <string.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define AP_TCP_PORT 11111
#define AP_UDP_PORT 22222
#define STA_TCP_PORT 33333
#define STA_UDP_PORT 44444

#define LISTEN_BACKLOG 5

bool fill_sockaddr_from_netif(esp_netif_t *netif, uint16_t port, struct sockaddr_in *addr_out);
void run_tcp_server(esp_netif_t *netif, uint16_t port);
void run_udp_server(esp_netif_t *netif, uint16_t port);

#endif // UM1_SOCKET_SERVER_H
