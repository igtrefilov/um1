#include "um1_router.h"
#include "um1_config.h"
#include "um1_mqtt.h"
#include "um1_lan.h"
#include "um1_uart.h"

#include <driver/uart.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

// ----------------- small vector -----------------

typedef struct { uint16_t *v; size_t n, cap; } vec_u16;

static inline void vec_push(vec_u16 *a, uint16_t x){
    if(a->n == a->cap){ a->cap = a->cap? a->cap*2 : 4;
        a->v = (uint16_t*)realloc(a->v, a->cap*sizeof(uint16_t)); }
    a->v[a->n++] = x;
}

// ----------------- index structures -----------------

typedef struct { uint16_t port; vec_u16 dsts; } port_bucket_t;

typedef struct {
    uint32_t ip;
    port_bucket_t *ports; size_t ports_n;
    vec_u16 any_port;
} ip_node_t;

typedef struct {
    proto_t proto;
    ip_node_t *ips; size_t ips_n;
    vec_u16 any_ip;
} proto_node_t;

typedef struct {
    char *topic;
    vec_u16 dsts;
} topic_node_t;

typedef struct {
    uint16_t src_id;
    proto_node_t *protos; size_t protos_n;
    vec_u16 any_proto;

    topic_node_t *topics; size_t topics_n;
    vec_u16 any_topic;
} src_index_t;

typedef struct {
    src_index_t *srcs; size_t srcs_n;
} router_index_t;

// ----------------- globals -----------------

static router_t        g_router = {0};
static router_index_t  g_index  = {0};

// ----------------- helpers -----------------

static src_index_t* find_src(router_index_t *idx, uint16_t src_id, bool create){
    for(size_t i=0;i<idx->srcs_n;i++)
        if(idx->srcs[i].src_id == src_id) return &idx->srcs[i];
    if(!create) return NULL;
    idx->srcs = realloc(idx->srcs, (idx->srcs_n+1)*sizeof(src_index_t));
    src_index_t *s = &idx->srcs[idx->srcs_n++];
    memset(s, 0, sizeof(*s));
    s->src_id = src_id;
    return s;
}

static proto_node_t* find_proto(src_index_t *s, proto_t p, bool create){
    for(size_t i=0;i<s->protos_n;i++)
        if(s->protos[i].proto == p) return &s->protos[i];
    if(!create) return NULL;
    s->protos = realloc(s->protos, (s->protos_n+1)*sizeof(proto_node_t));
    proto_node_t *pn = &s->protos[s->protos_n++];
    memset(pn,0,sizeof(*pn));
    pn->proto = p;
    return pn;
}

static ip_node_t* find_ip(proto_node_t *pn, uint32_t ip, bool create){
    for(size_t i=0;i<pn->ips_n;i++)
        if(pn->ips[i].ip == ip) return &pn->ips[i];
    if(!create) return NULL;
    pn->ips = realloc(pn->ips, (pn->ips_n+1)*sizeof(ip_node_t));
    ip_node_t *in = &pn->ips[pn->ips_n++];
    memset(in,0,sizeof(*in));
    in->ip = ip;
    return in;
}

static port_bucket_t* find_port(ip_node_t *in, uint16_t port, bool create){
    for(size_t i=0;i<in->ports_n;i++)
        if(in->ports[i].port == port) return &in->ports[i];
    if(!create) return NULL;
    in->ports = realloc(in->ports, (in->ports_n+1)*sizeof(port_bucket_t));
    port_bucket_t *pb = &in->ports[in->ports_n++];
    memset(pb,0,sizeof(*pb));
    pb->port = port;
    return pb;
}

static topic_node_t* find_topic(src_index_t *s, const char *topic, bool create){
    for(size_t i=0;i<s->topics_n;i++)
        if(strcmp(s->topics[i].topic, topic)==0) return &s->topics[i];
    if(!create) return NULL;
    s->topics = realloc(s->topics, (s->topics_n+1)*sizeof(topic_node_t));
    topic_node_t *tn = &s->topics[s->topics_n++];
    memset(tn,0,sizeof(*tn));
    tn->topic = strdup(topic);
    return tn;
}

// ----------------- build index -----------------

static void router_build_index(const router_t *r, router_index_t *idx){
    memset(idx, 0, sizeof(*idx));
    for(size_t i=0;i<r->routes_cnt;i++){
        const route_t *rt = &r->routes[i];
        if(!rt->active) continue;

        src_index_t *s = find_src(idx, rt->src_id, true);

        if((rt->match_flags & RMF_MATCH_PROTO) && rt->proto == PROTO_MQTT){
            if(rt->match_flags & RMF_MATCH_TOPIC){
                topic_node_t *tn = find_topic(s, rt->topic, true);
                vec_push(&tn->dsts, rt->dst_id);
            }else{
                vec_push(&s->any_topic, rt->dst_id);
            }
            continue;
        }

        if(rt->match_flags & RMF_MATCH_PROTO){
            proto_node_t *pn = find_proto(s, rt->proto, true);
            if(rt->match_flags & RMF_MATCH_IP){
                ip_node_t *in = find_ip(pn, rt->ip, true);
                if(rt->match_flags & RMF_MATCH_PORT){
                    port_bucket_t *pb = find_port(in, rt->port, true);
                    vec_push(&pb->dsts, rt->dst_id);
                }else{
                    vec_push(&in->any_port, rt->dst_id);
                }
            }else{
                vec_push(&pn->any_ip, rt->dst_id);
            }
        }else{
            vec_push(&s->any_proto, rt->dst_id);
        }
    }
}

// ----------------- endpoint lookup -----------------

static endpoint_t* router_find_ep(router_t *r, uint16_t id){
    for(size_t i=0;i<r->endpoints_cnt;i++)
        if(r->endpoints[i].id == id) return &r->endpoints[i];
    return NULL;
}

static void send_to_endpoint(const endpoint_t *dst,
                             const rx_meta_t *meta,
                             const uint8_t *data, size_t len){
    if(!dst || !dst->enable) return;
    switch(dst->iface.type){
    case IF_UART1:
        uart_write_bytes(UART_PORT_NUM_1, (const char*)data, len);
        break;
    case IF_UART2:
        uart_write_bytes(UART_PORT_NUM_2, (const char*)data, len);
        break;
    case IF_LAN:
    case IF_AP:
    case IF_STA:
        if(meta->proto == PROTO_UDP) send_udp_packet(data, len);
        else if(meta->proto == PROTO_MQTT) um1_mqtt_publish(data, len);
        else send_tcp_packet(data, len);
        break;
    }
}

static void send_vec(router_t *r, const vec_u16 *v,
                     const rx_meta_t *m, const uint8_t *data, size_t len){
    for(size_t i=0;i<v->n;i++){
        endpoint_t *dst = router_find_ep(r, v->v[i]);
        if(dst) send_to_endpoint(dst, m, data, len);
    }
}

static void route_data_fast(router_t *r, const router_index_t *idx,
                            const rx_meta_t *m, const uint8_t *data, size_t len){
    const src_index_t *s = NULL;
    for(size_t i=0;i<idx->srcs_n;i++) if(idx->srcs[i].src_id==m->src_id){ s=&idx->srcs[i]; break; }
    if(!s) return;

    if(m->proto == PROTO_MQTT){
        for(size_t i=0;i<s->topics_n;i++)
            if(strcmp(s->topics[i].topic, m->topic?m->topic:"")==0){
                send_vec(r, &s->topics[i].dsts, m, data, len);
                break;
            }
        send_vec(r, &s->any_topic, m, data, len);
        return;
    }

    const proto_node_t *pn = NULL;
    for(size_t i=0;i<s->protos_n;i++) if(s->protos[i].proto==m->proto){ pn=&s->protos[i]; break; }

    if(pn){
        for(size_t i=0;i<pn->ips_n;i++) if(pn->ips[i].ip==m->ip){
            const ip_node_t *in = &pn->ips[i];
            for(size_t j=0;j<in->ports_n;j++) if(in->ports[j].port==m->port){
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

// ----------------- configuration conversion -----------------

static iface_type_t iface_from_str(const char *s){
    if(!s) return 0;
    if(strcasecmp(s,"UART1")==0) return IF_UART1;
    if(strcasecmp(s,"UART2")==0) return IF_UART2;
    if(strcasecmp(s,"LAN")==0)   return IF_LAN;
    if(strcasecmp(s,"AP")==0)    return IF_AP;
    if(strcasecmp(s,"STA")==0)   return IF_STA;
    return 0;
}

static proto_t proto_from_str(const char *s){
    if(!s) return PROTO_NONE;
    if(strcasecmp(s,"TCP")==0) return PROTO_TCP;
    if(strcasecmp(s,"UDP")==0) return PROTO_UDP;
    if(strcasecmp(s,"MQTT")==0) return PROTO_MQTT;
    return PROTO_NONE;
}

void router_init_from_config(void){
    static endpoint_t eps[5];
    eps[0] = (endpoint_t){ .id=IF_UART1, .enable=true, .iface={.type=IF_UART1} };
    eps[1] = (endpoint_t){ .id=IF_UART2, .enable=true, .iface={.type=IF_UART2} };
    eps[2] = (endpoint_t){ .id=IF_LAN,   .enable=true, .iface={.type=IF_LAN} };
    eps[3] = (endpoint_t){ .id=IF_AP,    .enable=true, .iface={.type=IF_AP} };
    eps[4] = (endpoint_t){ .id=IF_STA,   .enable=true, .iface={.type=IF_STA} };

    g_router.endpoints = eps;
    g_router.endpoints_cnt = 5;

    static route_t routes[MAX_ROUTES];
    for(int i=0;i<global_route_count && i<MAX_ROUTES;i++){
        route_config_t *rc = &global_routes[i];
        route_t *rt = &routes[i];
        memset(rt,0,sizeof(*rt));
        rt->active = rc->active;
        rt->src_id = iface_from_str(rc->src.interface);
        rt->dst_id = iface_from_str(rc->dst.interface);
        if(rc->src.protocol[0]){ rt->match_flags |= RMF_MATCH_PROTO; rt->proto = proto_from_str(rc->src.protocol); }
        if(rc->src.ip[0]){ rt->match_flags |= RMF_MATCH_IP; rt->ip = inet_addr(rc->src.ip); }
        if(rc->src.port){ rt->match_flags |= RMF_MATCH_PORT; rt->port = (uint16_t)rc->src.port; }
        if(rc->src.topic[0]){ rt->match_flags |= RMF_MATCH_TOPIC; rt->topic = strdup(rc->src.topic); }
    }
    g_router.routes = routes;
    g_router.routes_cnt = global_route_count < MAX_ROUTES ? global_route_count : MAX_ROUTES;

    router_build_index(&g_router, &g_index);
}

void router_route(const rx_meta_t *m, const uint8_t *data, size_t len){
    route_data_fast(&g_router, &g_index, m, data, len);
}

