#ifndef UM1_ROUTER_H
#define UM1_ROUTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// --- enums for interfaces and protocols ---

typedef enum { IF_UART1 = 1, IF_UART2 = 2, IF_LAN = 3, IF_AP = 4, IF_STA = 5 } iface_type_t;

typedef enum { PROTO_NONE = 0, PROTO_TCP = 1, PROTO_UDP = 2, PROTO_MQTT = 3 } proto_t;

typedef enum {
    ROLE_NONE = 0,
    ROLE_CLIENT = 1,
    ROLE_SERVER = 2,
    ROLE_PUBLISH = 3,
    ROLE_SUBSCRIBE = 4
} role_t;

typedef enum { PARITY_NONE = 0, PARITY_EVEN = 1, PARITY_ODD = 2 } parity_t;
typedef enum { AUTH_OPEN = 0, AUTH_WPA2 = 1 } auth_t;

// --- interface configurations ---

typedef struct {
    uint32_t baudrate;
    parity_t parity;
    uint8_t  bits;
} uart_cfg_t;

typedef struct {
    role_t   role;
    proto_t  proto;
    uint32_t ip;
    uint16_t port;
    const char *topic;
} net_cfg_t;

typedef struct { net_cfg_t net; } lan_cfg_t;

typedef struct {
    const char *ssid;
    const char *password;
    auth_t auth;
    net_cfg_t net;
} wifi_cfg_t;

typedef struct {
    iface_type_t type;
    union {
        uart_cfg_t uart1;
        uart_cfg_t uart2;
        lan_cfg_t  lan;
        wifi_cfg_t ap;
        wifi_cfg_t sta;
    } u;
} phy_iface_t;

typedef struct {
    uint16_t    id;
    bool        enable;
    phy_iface_t iface;
} endpoint_t;

typedef enum {
    RMF_MATCH_PROTO = 1<<0,
    RMF_MATCH_IP    = 1<<1,
    RMF_MATCH_PORT  = 1<<2,
    RMF_MATCH_TOPIC = 1<<3,
} route_match_flags_t;

typedef struct {
    bool        active;
    uint16_t    src_id;
    uint16_t    dst_id;
    uint32_t    match_flags;
    proto_t     proto;
    uint32_t    ip;
    uint16_t    port;
    const char *topic;
} route_t;

typedef struct {
    endpoint_t *endpoints; size_t endpoints_cnt;
    route_t    *routes;    size_t routes_cnt;
} router_t;

typedef struct {
    uint16_t    src_id;
    proto_t     proto;
    uint32_t    ip;
    uint16_t    port;
    const char *topic;
} rx_meta_t;

void router_init_from_config(void);
void router_route(const rx_meta_t *m, const uint8_t *data, size_t len);

#endif // UM1_ROUTER_H
