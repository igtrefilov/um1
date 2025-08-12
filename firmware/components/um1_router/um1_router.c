#include "um1_router.h"
#include "um1_config.h"
#include "um1_mqtt.h"
#include "um1_lan.h"
#include <driver/uart.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <lwip/inet.h>

#define UART_PORT_NUM_1    1
#define UART_PORT_NUM_2    2

/*
 * Simple dynamic array used for storing endpoint ids in the index.  In a real
 * project this can be replaced with a proper container (uthash/khash/etc).
 */
typedef struct { uint16_t *v; size_t n, cap; } vec_u16;

static inline void vec_push(vec_u16 *a, uint16_t x)
{
    if (a->n == a->cap) {
        a->cap = a->cap ? a->cap * 2 : 4;
        a->v = (uint16_t *)realloc(a->v, a->cap * sizeof(uint16_t));
    }
    a->v[a->n++] = x;
}

/*
 * Index node declarations ----------------------------------------------------
 */
typedef struct { uint16_t port; vec_u16 dsts; } port_bucket_t;

typedef struct {
    uint32_t ip;
    port_bucket_t *ports; size_t ports_n;
    vec_u16 any_port;                     /* rules without port */
} ip_node_t;

typedef struct {
    proto_t proto;
    ip_node_t *ips; size_t ips_n;
    vec_u16 any_ip;                       /* rules without IP */
} proto_node_t;

typedef struct {
    char *topic;                          /* MQTT topic */
    vec_u16 dsts;
} topic_node_t;

typedef struct {
    uint16_t src_id;
    proto_node_t *protos; size_t protos_n;
    vec_u16 any_proto;                    /* rules without proto */

    topic_node_t *topics; size_t topics_n;/* MQTT branch */
    vec_u16 any_topic;                    /* MQTT without topic */
} src_index_t;

typedef struct {
    src_index_t *srcs; size_t srcs_n;
} router_index_t;

/*
 * Router data model ---------------------------------------------------------
 */
typedef struct {
    iface_type_t type;
    union {
        struct { /* UART config placeholder */ } uart1;
        struct { /* UART config placeholder */ } uart2;
        struct { proto_t proto; const char *topic; } net; /* minimal net cfg */
    } u;
} phy_iface_t;

typedef struct {
    uint16_t id;
    bool     enable;
    phy_iface_t iface;
} endpoint_t;

typedef enum {
    RMF_MATCH_PROTO = 1 << 0,
    RMF_MATCH_IP    = 1 << 1,
    RMF_MATCH_PORT  = 1 << 2,
    RMF_MATCH_TOPIC = 1 << 3,
} route_match_flags_t;

typedef struct {
    bool        active;
    uint16_t    src_id;
    uint16_t    dst_id;
    uint32_t    match_flags;
    proto_t     proto;
    uint32_t    ip;
    uint16_t    port;
    char       *topic;      /* duplicated if RMF_MATCH_TOPIC */
} route_t;

typedef struct {
    endpoint_t *endpoints; size_t endpoints_cnt;
    route_t    *routes;    size_t routes_cnt;
} router_t;

static router_t router;
static router_index_t router_idx;

static endpoint_t endpoints[6];
static route_t routes[MAX_ROUTES];

/*
 * Utility conversion helpers -------------------------------------------------
 */
static uint16_t iface_to_id(const char *s)
{
    if (!s) return 0;
    if (strcasecmp(s, "UART1") == 0) return 1;
    if (strcasecmp(s, "UART2") == 0) return 2;
    if (strcasecmp(s, "LAN")   == 0) return 3;
    if (strcasecmp(s, "AP")    == 0) return 4;
    if (strcasecmp(s, "STA")   == 0) return 5;
    if (strcasecmp(s, "MQTT")  == 0) return 6;
    return 0;
}

static proto_t parse_proto(const char *s)
{
    if (!s || !*s) return PROTO_NONE;
    if (strcasecmp(s, "TCP")  == 0) return PROTO_TCP;
    if (strcasecmp(s, "UDP")  == 0) return PROTO_UDP;
    if (strcasecmp(s, "MQTT") == 0) return PROTO_MQTT;
    return PROTO_NONE;
}

static uint32_t parse_ip(const char *s)
{
    if (!s || !*s) return 0;
    return inet_addr(s); /* network order */
}

/*
 * find-or-add helpers -------------------------------------------------------
 */
static src_index_t *find_src(router_index_t *idx, uint16_t src_id, bool create)
{
    for (size_t i = 0; i < idx->srcs_n; ++i)
        if (idx->srcs[i].src_id == src_id)
            return &idx->srcs[i];
    if (!create) return NULL;
    idx->srcs = (src_index_t *)realloc(idx->srcs,
                                       (idx->srcs_n + 1) * sizeof(src_index_t));
    src_index_t *s = &idx->srcs[idx->srcs_n++];
    memset(s, 0, sizeof(*s));
    s->src_id = src_id;
    return s;
}

static proto_node_t *find_proto(src_index_t *s, proto_t p, bool create)
{
    for (size_t i = 0; i < s->protos_n; ++i)
        if (s->protos[i].proto == p)
            return &s->protos[i];
    if (!create) return NULL;
    s->protos = (proto_node_t *)realloc(s->protos,
                                        (s->protos_n + 1) * sizeof(proto_node_t));
    proto_node_t *pn = &s->protos[s->protos_n++];
    memset(pn, 0, sizeof(*pn));
    pn->proto = p;
    return pn;
}

static ip_node_t *find_ip(proto_node_t *pn, uint32_t ip, bool create)
{
    for (size_t i = 0; i < pn->ips_n; ++i)
        if (pn->ips[i].ip == ip)
            return &pn->ips[i];
    if (!create) return NULL;
    pn->ips = (ip_node_t *)realloc(pn->ips,
                                   (pn->ips_n + 1) * sizeof(ip_node_t));
    ip_node_t *in = &pn->ips[pn->ips_n++];
    memset(in, 0, sizeof(*in));
    in->ip = ip;
    return in;
}

static port_bucket_t *find_port(ip_node_t *in, uint16_t port, bool create)
{
    for (size_t i = 0; i < in->ports_n; ++i)
        if (in->ports[i].port == port)
            return &in->ports[i];
    if (!create) return NULL;
    in->ports = (port_bucket_t *)realloc(in->ports,
                                         (in->ports_n + 1) * sizeof(port_bucket_t));
    port_bucket_t *pb = &in->ports[in->ports_n++];
    memset(pb, 0, sizeof(*pb));
    pb->port = port;
    return pb;
}

static topic_node_t *find_topic(src_index_t *s, const char *topic, bool create)
{
    for (size_t i = 0; i < s->topics_n; ++i)
        if (strcmp(s->topics[i].topic, topic ? topic : "") == 0)
            return &s->topics[i];
    if (!create) return NULL;
    s->topics = (topic_node_t *)realloc(s->topics,
                                        (s->topics_n + 1) * sizeof(topic_node_t));
    topic_node_t *tn = &s->topics[s->topics_n++];
    memset(tn, 0, sizeof(*tn));
    tn->topic = strdup(topic ? topic : "");
    return tn;
}

/*
 * Index construction --------------------------------------------------------
 */
static void router_build_index(const router_t *r, router_index_t *idx)
{
    memset(idx, 0, sizeof(*idx));
    for (size_t i = 0; i < r->routes_cnt; i++) {
        const route_t *rt = &r->routes[i];
        if (!rt->active) continue;

        src_index_t *s = find_src(idx, rt->src_id, true);

        if ((rt->match_flags & RMF_MATCH_PROTO) && rt->proto == PROTO_MQTT) {
            if (rt->match_flags & RMF_MATCH_TOPIC) {
                topic_node_t *tn = find_topic(s, rt->topic, true);
                vec_push(&tn->dsts, rt->dst_id);
            } else {
                vec_push(&s->any_topic, rt->dst_id);
            }
            continue;
        }

        if (rt->match_flags & RMF_MATCH_PROTO) {
            proto_node_t *pn = find_proto(s, rt->proto, true);
            if (rt->match_flags & RMF_MATCH_IP) {
                ip_node_t *in = find_ip(pn, rt->ip, true);
                if (rt->match_flags & RMF_MATCH_PORT) {
                    port_bucket_t *pb = find_port(in, rt->port, true);
                    vec_push(&pb->dsts, rt->dst_id);
                } else {
                    vec_push(&in->any_port, rt->dst_id);
                }
            } else {
                vec_push(&pn->any_ip, rt->dst_id);
            }
        } else {
            vec_push(&s->any_proto, rt->dst_id);
        }
    }
}

/*
 * Delivery helpers ----------------------------------------------------------
 */
static endpoint_t *router_find_ep(router_t *r, uint16_t id)
{
    for (size_t i = 0; i < r->endpoints_cnt; i++)
        if (r->endpoints[i].id == id)
            return &r->endpoints[i];
    return NULL;
}

static void send_to_endpoint(const endpoint_t *dst, const rx_meta_t *meta,
                             const uint8_t *data, size_t len)
{
    switch (dst->iface.type) {
    case IF_UART1:
        uart_write_bytes(UART_PORT_NUM_1, (const char *)data, len);
        break;
    case IF_UART2:
        uart_write_bytes(UART_PORT_NUM_2, (const char *)data, len);
        break;
    case IF_LAN:
    case IF_AP:
    case IF_STA: {
        /* MQTT takes precedence if configured */
        if (dst->iface.u.net.proto == PROTO_MQTT) {
            esp_mqtt_client_handle_t client = get_mqtt_client_handle();
            if (client && global_mqtt_config.enabled && global_mqtt_config.tx_enabled) {
                const char *topic = dst->iface.u.net.topic ? dst->iface.u.net.topic : "data";
                esp_mqtt_client_publish(client, topic, (const char *)data, len, 1, 0);
            }
            break;
        }

        int src_uart_port = -1;
        if (meta->src_id == iface_to_id("UART1")) src_uart_port = UART_PORT_NUM_1;
        else if (meta->src_id == iface_to_id("UART2")) src_uart_port = UART_PORT_NUM_2;

        if (global_tcp_config.enabled)
            send_tcp_packet(src_uart_port, data, len);
        if (global_udp_config.enabled)
            send_udp_packet(src_uart_port, data, len);
        break;
    }
    }
}

static void send_vec(router_t *r, const vec_u16 *v,
                     const rx_meta_t *m, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < v->n; i++) {
        endpoint_t *dst = router_find_ep(r, v->v[i]);
        if (dst && dst->enable)
            send_to_endpoint(dst, m, data, len);
    }
}

static void route_data_fast(router_t *r, const router_index_t *idx,
                            const rx_meta_t *m, const uint8_t *data, size_t len)
{
    const src_index_t *s = NULL;
    for (size_t i = 0; i < idx->srcs_n; i++)
        if (idx->srcs[i].src_id == m->src_id) { s = &idx->srcs[i]; break; }
    if (!s) return;

    if (m->proto == PROTO_MQTT) {
        for (size_t i = 0; i < s->topics_n; i++)
            if (strcmp(s->topics[i].topic, m->topic ? m->topic : "") == 0) {
                send_vec(r, &s->topics[i].dsts, m, data, len);
                break;
            }
        send_vec(r, &s->any_topic, m, data, len);
        return;
    }

    const proto_node_t *pn = NULL;
    for (size_t i = 0; i < s->protos_n; i++)
        if (s->protos[i].proto == m->proto) { pn = &s->protos[i]; break; }

    if (pn) {
        for (size_t i = 0; i < pn->ips_n; i++)
            if (pn->ips[i].ip == m->ip) {
                const ip_node_t *in = &pn->ips[i];
                for (size_t j = 0; j < in->ports_n; j++)
                    if (in->ports[j].port == m->port) {
                        send_vec(r, &in->ports[j].dsts, m, data, len);
                        break;
                    }
                send_vec(r, &in->any_port, m, data, len);
                goto done_proto;
            }
        send_vec(r, &pn->any_ip, m, data, len);
    }
done_proto:
    send_vec(r, &s->any_proto, m, data, len);
}

/*
 * Public API ----------------------------------------------------------------
 */
void router_init(void)
{
    memset(endpoints, 0, sizeof(endpoints));
    endpoints[0] = (endpoint_t){ .id = 1, .enable = true, .iface = { .type = IF_UART1 } };
    endpoints[1] = (endpoint_t){ .id = 2, .enable = true, .iface = { .type = IF_UART2 } };
    endpoints[2] = (endpoint_t){ .id = 3, .enable = true, .iface = { .type = IF_LAN } };
    endpoints[3] = (endpoint_t){ .id = 4, .enable = true, .iface = { .type = IF_AP } };
    endpoints[4] = (endpoint_t){ .id = 5, .enable = true, .iface = { .type = IF_STA } };
    endpoints[5] = (endpoint_t){ .id = 6, .enable = true,
                                .iface = { .type = IF_STA,
                                           .u.net = { .proto = PROTO_MQTT,
                                                      .topic = global_mqtt_config.topic[0] ?
                                                               global_mqtt_config.topic : NULL } } };

    router.endpoints = endpoints;
    router.endpoints_cnt = sizeof(endpoints)/sizeof(endpoints[0]);
    router.routes = routes;
    router.routes_cnt = 0;

    for (int i = 0; i < global_route_count && i < MAX_ROUTES; ++i) {
        route_config_t *cfg = &global_routes[i];
        route_t *rt = &routes[router.routes_cnt];
        memset(rt, 0, sizeof(*rt));
        rt->active = cfg->active;
        rt->src_id = iface_to_id(cfg->src.interface);
        rt->dst_id = iface_to_id(cfg->dst.interface);
        if (cfg->src.protocol[0]) {
            rt->match_flags |= RMF_MATCH_PROTO;
            rt->proto = parse_proto(cfg->src.protocol);
        }
        if (cfg->src.ip[0]) {
            rt->match_flags |= RMF_MATCH_IP;
            rt->ip = parse_ip(cfg->src.ip);
        }
        if (cfg->src.port) {
            rt->match_flags |= RMF_MATCH_PORT;
            rt->port = cfg->src.port;
        }
        if (cfg->src.topic[0]) {
            rt->match_flags |= RMF_MATCH_TOPIC;
            rt->topic = strdup(cfg->src.topic);
        }
        router.routes_cnt++;
    }

    router_build_index(&router, &router_idx);
}

void route_data(const char *src_if, const uint8_t *data, size_t len)
{
    rx_meta_t m = {0};
    m.src_id = iface_to_id(src_if);
    m.proto = PROTO_NONE;
    m.ip = 0;
    m.port = 0;
    m.topic = NULL;
    route_data_fast(&router, &router_idx, &m, data, len);
}

void route_data_meta(const rx_meta_t *meta, const uint8_t *data, size_t len)
{
    route_data_fast(&router, &router_idx, meta, data, len);
}

