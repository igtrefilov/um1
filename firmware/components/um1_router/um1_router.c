#include "um1_router.h"
#include "um1_http_server.h"

static const char *TAG = "um1_router";

typedef enum { IF_NONE=0, IF_UART1, IF_UART2, IF_LAN, IF_AP, IF_STA, IF_MQTT } if_t;
typedef enum { TP_NONE=0, TP_TCP, TP_UDP } tp_t;

typedef struct {
    bool     is_client;
    char     addr[64];
    int      port;
    tp_t     tp;
} ip_prof_t;

typedef struct {
    if_t      ifc;
    int       uart_port;
    ip_prof_t ip;
    char      tx_topic[64];
    char      rx_topic[64];
} endpoint_t;

#define MAX_MON_TCP_CLIENTS 4

typedef struct {
    int  listen_fd;
    int  clients[MAX_MON_TCP_CLIENTS];
} mon_tcp_srv_t;

typedef struct {
    int                udp_sock;
    struct sockaddr_in udp_peers[8];
    size_t             udp_peer_cnt;
} mon_udp_srv_t;

typedef struct {
    int                udp_sock;
    struct sockaddr_in udp_peers[8];
    size_t             udp_peer_cnt;
} gate_udp_srv_t;

typedef struct {
    int sock;
} ip_client_t;

typedef struct {
    int listen_fd;
    int client_fd;
    struct sockaddr_in peer;
} ip_server_t;

typedef struct {
    endpoint_t    gate;
    ip_client_t   gate_ip_cli;
    ip_server_t   gate_ip_srv;
    gate_udp_srv_t gate_udp;
    endpoint_t    mon;
    mon_tcp_srv_t mon_tcp;
    mon_udp_srv_t mon_udp;
} rt_ctx_t;

static rt_ctx_t g_ctx[2];

static esp_netif_t *g_lan = NULL, *g_ap = NULL, *g_sta = NULL;
esp_netif_t *router_get_netif_lan(void){ return g_lan; }
esp_netif_t *router_get_netif_ap(void){ return g_ap; }
esp_netif_t *router_get_netif_sta(void){ return g_sta; }
void router_set_netif_lan(esp_netif_t *n){ g_lan = n; }
void router_set_netif_ap(esp_netif_t *n){ g_ap  = n; }
void router_set_netif_sta(esp_netif_t *n){ g_sta = n; }

static const char* ifc_name(if_t ifc) {
    switch (ifc) {
        case IF_LAN: return "LAN";
        case IF_AP:  return "AP";
        case IF_STA: return "STA";
        case IF_UART1: return "UART1";
        case IF_UART2: return "UART2";
        case IF_MQTT: return "MQTT";
        default: return "?";
    }
}

static esp_netif_t *netif_for_ifc(if_t ifc) {
    switch (ifc) {
        case IF_LAN: return router_get_netif_lan();
        case IF_AP:  return router_get_netif_ap();
        case IF_STA: return router_get_netif_sta();
        default:     return NULL;
    }
}

static inline bool is_ip_interface(if_t ifc){
    return (ifc==IF_LAN || ifc==IF_AP || ifc==IF_STA);
}

static in_addr_t ip_of_if(if_t ifc) {
    esp_netif_t *n = netif_for_ifc(ifc);
    if (!n) return htonl(INADDR_ANY);
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(n, &ip) == ESP_OK) return ip.ip.addr;
    return htonl(INADDR_ANY);
}

static bool bind_local_to_if(int sock, if_t ifc) {
    in_addr_t ip = ip_of_if(ifc);
    if (ip == htonl(INADDR_ANY)) return false;
    struct sockaddr_in l = {0};
    l.sin_family = AF_INET;
    l.sin_port   = htons(0);
    l.sin_addr.s_addr = ip;
    if (bind(sock, (struct sockaddr*)&l, sizeof(l)) < 0) {
        ESP_LOGE(TAG, "bind(local-if) failed, err=%d", errno);
        return false;
    }
    return true;
}

static bool wait_ip_ready(if_t ifc, TickType_t delay_ms){
    if (!is_ip_interface(ifc)) return true;
    in_addr_t ip = ip_of_if(ifc);
    if (ip == htonl(INADDR_ANY)) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        return false;
    }
    return true;
}

static inline int ci_strncmp(const char *a, const char *b, size_t n){
    while (n && *a && *b){
        char ca = (*a>='A' && *a<='Z')? *a+32 : *a;
        char cb = (*b>='A' && *b<='Z')? *b+32 : *b;
        if (ca!=cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
        ++a; ++b; --n;
    }
    if (n==0) return 0;
    return *a - *b;
}

static inline int ci_strcmp(const char *a, const char *b){
    while (*a && *b){
        char ca = (*a>='A' && *a<='Z')? *a+32 : *a;
        char cb = (*b>='A' && *b<='Z')? *b+32 : *b;
        if (ca!=cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
        ++a; ++b;
    }
    return (*a?1:0) - (*b?1:0);
}

static if_t parse_ifc(const char *s){
    if (!s) return IF_NONE;
    if (!ci_strcmp(s,"lan"))   return IF_LAN;
    if (!ci_strcmp(s,"ap"))    return IF_AP;
    if (!ci_strcmp(s,"sta"))   return IF_STA;
    if (!ci_strcmp(s,"uart1")) return IF_UART1;
    if (!ci_strcmp(s,"uart2")) return IF_UART2;
    return IF_NONE;
}

static tp_t parse_tp(const char *s){
    if (!s) return TP_NONE;
    if (!ci_strcmp(s,"tcp")) return TP_TCP;
    if (!ci_strcmp(s,"udp")) return TP_UDP;
    return TP_NONE;
}

static void fill_endpoint_from_route(const route_item_t *ri, endpoint_t *ep){
    memset(ep,0,sizeof(*ep));
    if (!ri) return;
    if (!ci_strcmp(ri->intf, "uart1")) { ep->ifc = IF_UART1; ep->uart_port = 1; return; }
    if (!ci_strcmp(ri->intf, "uart2")) { ep->ifc = IF_UART2; ep->uart_port = 2; return; }
    if (ri->profile[0] && !ci_strncmp(ri->profile, "mqtt", 4)) {
        ep->ifc = IF_MQTT;
        int midx = (!ci_strcmp(ri->profile,"mqtt2"))?1:0;
        const mqtt_profile_t *mp = &global_mqtt_profiles[midx];
        strncpy(ep->tx_topic, mp->tx_topic, sizeof(ep->tx_topic)-1);
        strncpy(ep->rx_topic, mp->rx_topic, sizeof(ep->rx_topic)-1);
        return;
    }
    ep->ifc = parse_ifc(ri->intf);
    if (ep->ifc==IF_LAN || ep->ifc==IF_AP || ep->ifc==IF_STA){
        int pidx = (!ci_strcmp(ri->profile,"ip2"))?1:0;
        const ip_profile_t *p = &global_ip_profiles[pidx];
        ep->ip.is_client = p->client;
        strncpy(ep->ip.addr, p->address, sizeof(ep->ip.addr)-1);
        ep->ip.port = p->port;
        ep->ip.tp   = parse_tp(p->transport);
    }
}

static void mon_send_udp(rt_ctx_t *ctx, const uint8_t *data, size_t len){
    if (!ctx) return;
    if (!is_ip_interface(ctx->mon.ifc)) return;
    if (ctx->mon.ip.tp!=TP_UDP || ctx->mon_udp.udp_sock<0) return;
    for(size_t i=0;i<ctx->mon_udp.udp_peer_cnt;i++){
        sendto(ctx->mon_udp.udp_sock, data, len, 0,
               (struct sockaddr*)&ctx->mon_udp.udp_peers[i], sizeof(struct sockaddr_in));
    }
}

static void mon_send_tcp(rt_ctx_t *ctx, const uint8_t *data, size_t len){
    if (!ctx) return;
    if (ctx->mon_tcp.listen_fd<0) return;
    for(int i=0;i<MAX_MON_TCP_CLIENTS;i++){
        int fd = ctx->mon_tcp.clients[i];
        if (fd>=0) send(fd, data, len, 0);
    }
}

static void mon_send_mqtt(rt_ctx_t *ctx, const uint8_t *data, size_t len){
    if (!ctx) return;
    if (ctx->mon.ifc!=IF_MQTT || ctx->mon.tx_topic[0]=='\0') return;
    esp_mqtt_client_handle_t c = get_mqtt_client_handle();
    if (c) esp_mqtt_client_publish(c, ctx->mon.tx_topic, (const char*)data, (int)len, 1, 0);
}

static bool mon_cfg_equal(const rt_ctx_t *a, const rt_ctx_t *b){
    if (!a || !b) return false;
    if (a->mon.ifc != b->mon.ifc) return false;
    if (a->mon.ifc == IF_MQTT) {
        return strcmp(a->mon.tx_topic, b->mon.tx_topic) == 0 &&
               strcmp(a->mon.rx_topic, b->mon.rx_topic) == 0;
    }
    if (is_ip_interface(a->mon.ifc)){
        return a->mon.ip.tp == b->mon.ip.tp && a->mon.ip.port == b->mon.ip.port;
    }
    return false;
}

static void mon_tee_matching(const rt_ctx_t *src_ctx, const uint8_t *data, size_t len){
    bool udp_done=false, tcp_done=false, mqtt_done=false;
    for (int i=0;i<2;i++){
        rt_ctx_t *dst = &g_ctx[i];
        if (!mon_cfg_equal(src_ctx, dst)) continue;
        if (is_ip_interface(dst->mon.ifc) &&
            dst->mon.ip.tp==TP_UDP && !udp_done && dst->mon_udp.udp_sock>=0){
            mon_send_udp(dst, data, len);
            udp_done = true;
        }
        if (is_ip_interface(dst->mon.ifc) &&
            dst->mon.ip.tp==TP_TCP && !tcp_done && dst->mon_tcp.listen_fd>=0){
            mon_send_tcp(dst, data, len);
            tcp_done = true;
        }
        if (dst->mon.ifc==IF_MQTT && !mqtt_done && dst->mon.tx_topic[0]){
            mon_send_mqtt(dst, data, len);
            mqtt_done = true;
        }
    }
}

static void gate_send_uart(int uart_port, const uint8_t *data, size_t len){
    int port = (uart_port==2)?UART_PORT_NUM_2:UART_PORT_NUM_1;
    uart_write_bytes(port, (const char*)data, len);
    uart_wait_tx_done(port, portMAX_DELAY);
}

static void gate_send_ip(rt_ctx_t *ctx, const uint8_t *data, size_t len){
    if (!ctx) return;
    if (ctx->gate.ip.tp==TP_TCP){
        int fd = ctx->gate.ip.is_client? ctx->gate_ip_cli.sock : ctx->gate_ip_srv.client_fd;
        if (fd>=0) send(fd, data, len, 0);
    }else if (ctx->gate.ip.tp==TP_UDP){
        if (ctx->gate.ip.is_client){
            int sock = ctx->gate_ip_cli.sock;
            if (sock>=0){
                struct sockaddr_in to={0};
                to.sin_family=AF_INET;
                to.sin_port=htons(ctx->gate.ip.port);
                to.sin_addr.s_addr=inet_addr(ctx->gate.ip.addr);
                sendto(sock,data,len,0,(struct sockaddr*)&to,sizeof(to));
            }
        }else{
            if (ctx->gate_udp.udp_sock>=0){
                for(size_t i=0;i<ctx->gate_udp.udp_peer_cnt;i++){
                    sendto(ctx->gate_udp.udp_sock, data, len, 0,
                           (struct sockaddr*)&ctx->gate_udp.udp_peers[i], sizeof(struct sockaddr_in));
                }
            }
        }
    }
}

static void gate_send_mqtt(rt_ctx_t *ctx, const uint8_t *data, size_t len){
    if (!ctx || ctx->gate.tx_topic[0]=='\0') return;
    esp_mqtt_client_handle_t c = get_mqtt_client_handle();
    if (c) esp_mqtt_client_publish(c, ctx->gate.tx_topic, (const char*)data, (int)len, 1, 0);
}

void router_on_uart_rx(int uart_port, const uint8_t *data, size_t len){
    int idx = (uart_port==2)?1:0;
    rt_ctx_t *ctx = &g_ctx[idx];
    switch(ctx->gate.ifc){
        case IF_UART1:
        case IF_UART2:{
            int other = (ctx->gate.uart_port==2)?2:1;
            gate_send_uart(other, data, len);
            char msg[128];
            snprintf(msg, sizeof(msg), "[UART%d]->[UART%d]", uart_port, other);
            size_t msg_len = strlen(msg);
            uint8_t *pay = malloc(4 + msg_len + 1);
            if (pay) {
                pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                memcpy(pay+4, msg, msg_len+1);
                send_uart_ws_data(uart_port, pay, 4 + msg_len + 1);
                free(pay);
            }
        }break;
        case IF_LAN:
        case IF_AP:
        case IF_STA:{
            gate_send_ip(ctx, data, len);
            char ipbuf[16] = "";
            int rport = 0;
            if (ctx->gate.ip.tp == TP_TCP) {
                if (ctx->gate.ip.is_client) {
                    strncpy(ipbuf, ctx->gate.ip.addr, sizeof(ipbuf)-1);
                    rport = ctx->gate.ip.port;
                } else {
                    if (ctx->gate_ip_srv.client_fd >= 0) {
                        inet_ntoa_r(ctx->gate_ip_srv.peer.sin_addr, ipbuf, sizeof(ipbuf));
                        rport = ntohs(ctx->gate_ip_srv.peer.sin_port);
                    }
                }
                char msg[160];
                snprintf(msg, sizeof(msg), "[UART%d]->[%s][%s][%d][TCP]", uart_port, ifc_name(ctx->gate.ifc), ipbuf, rport);
                size_t msg_len = strlen(msg);
                uint8_t *pay = malloc(4 + msg_len + 1);
                if (pay) {
                    pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                    memcpy(pay+4, msg, msg_len+1);
                    send_uart_ws_data(uart_port, pay, 4 + msg_len + 1);
                    free(pay);
                }
            } else {
                char msg[160];
                snprintf(msg, sizeof(msg), "[UART%d]->[%s][%s][%d][UDP]", uart_port, ifc_name(ctx->gate.ifc), ctx->gate.ip.addr, ctx->gate.ip.port);
                size_t msg_len = strlen(msg);
                uint8_t *pay = malloc(4 + msg_len + 1);
                if (pay) {
                    pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                    memcpy(pay+4, msg, msg_len+1);
                    send_uart_ws_data(uart_port, pay, 4 + msg_len + 1);
                    free(pay);
                }
            }
        }break;
        case IF_MQTT:{
            gate_send_mqtt(ctx, data, len);
            char msg[64];
            snprintf(msg, sizeof(msg), "[UART%d]->[MQTT]", uart_port);
            size_t msg_len = strlen(msg);
            uint8_t *pay = malloc(4 + msg_len + 1);
            if (pay) {
                pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                memcpy(pay+4, msg, msg_len+1);
                send_uart_ws_data(uart_port, pay, 4 + msg_len + 1);
                free(pay);
            }
        }break;
        default: break;
    }
    mon_tee_matching(ctx, data, len);
}

void router_on_mqtt_message(const char *topic, const uint8_t *data, size_t len){
    for (int i=0;i<2;i++){
        rt_ctx_t *ctx=&g_ctx[i];
        if (ctx->gate.ifc==IF_MQTT && ctx->gate.rx_topic[0] && strcmp(topic, ctx->gate.rx_topic)==0){
            int u = i==1?2:1;
            gate_send_uart(u, data, len);
            char msg[64];
            snprintf(msg, sizeof(msg), "[MQTT]->[UART%d]", u);
            size_t msg_len = strlen(msg);
            uint8_t *pay = malloc(4 + msg_len + 1);
            if (pay) {
                pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                memcpy(pay+4, msg, msg_len+1);
                send_uart_ws_data(u, pay, 4 + msg_len + 1);
                free(pay);
            }
            mon_tee_matching(ctx, data, len);
        }
    }
}

static void mon_udp_server_task(void *pv){
    rt_ctx_t *ctx = (rt_ctx_t*)pv;
    for(;;){
        if (!wait_ip_ready(ctx->mon.ifc, 500)) continue;
        ctx->mon_udp.udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        int s = ctx->mon_udp.udp_sock;
        if (s<0){ vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        int yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        struct sockaddr_in bindaddr={0};
        bindaddr.sin_family=AF_INET;
        bindaddr.sin_port=htons(ctx->mon.ip.port);
        bindaddr.sin_addr.s_addr = ip_of_if(ctx->mon.ifc);
        if (bind(s,(struct sockaddr*)&bindaddr,sizeof(bindaddr)) < 0){
            ESP_LOGE(TAG,"MON-UDP: bind(%s:%d) failed, err=%d",
                     inet_ntoa(*(struct in_addr*)&bindaddr.sin_addr.s_addr), ctx->mon.ip.port, errno);
            close(s); ctx->mon_udp.udp_sock = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        ESP_LOGI(TAG,"MON-UDP[%d]: listening on %s:%d",
                 (ctx==&g_ctx[1])?2:1, inet_ntoa(*(struct in_addr*)&bindaddr.sin_addr.s_addr), ctx->mon.ip.port);
        ctx->mon_udp.udp_peer_cnt = 0;
        for(;;){
            uint8_t buf[1500];
            struct sockaddr_in from; socklen_t fl=sizeof(from);
            ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fl);
            if (n>0){
                bool known=false;
                for(size_t i=0;i<ctx->mon_udp.udp_peer_cnt;i++){
                    if (ctx->mon_udp.udp_peers[i].sin_addr.s_addr==from.sin_addr.s_addr &&
                        ctx->mon_udp.udp_peers[i].sin_port==from.sin_port){ known=true; break; }
                }
                if(!known && ctx->mon_udp.udp_peer_cnt<sizeof(ctx->mon_udp.udp_peers)/sizeof(ctx->mon_udp.udp_peers[0])){
                    ctx->mon_udp.udp_peers[ctx->mon_udp.udp_peer_cnt++] = from;
                }
            } else if (n<0) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

static void mon_tcp_server_task(void *pv){
    rt_ctx_t *ctx=(rt_ctx_t*)pv;
    for(;;){
        if (!wait_ip_ready(ctx->mon.ifc, 500)) continue;
        int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (s<0){ vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        ctx->mon_tcp.listen_fd = s;
        int yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        struct sockaddr_in a={0};
        a.sin_family=AF_INET; a.sin_port=htons(ctx->mon.ip.port);
        a.sin_addr.s_addr = ip_of_if(ctx->mon.ifc);
        if (bind(s,(struct sockaddr*)&a,sizeof(a)) < 0) {
            ESP_LOGE(TAG,"MON-TCP: bind(%s:%d) failed, err=%d",
                     inet_ntoa(*(struct in_addr*)&a.sin_addr.s_addr), ctx->mon.ip.port, errno);
            close(s); ctx->mon_tcp.listen_fd = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (listen(s, 4) < 0) {
            ESP_LOGE(TAG,"MON-TCP: listen(:%d) failed, err=%d", ctx->mon.ip.port, errno);
            close(s); ctx->mon_tcp.listen_fd = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        for(int i=0;i<MAX_MON_TCP_CLIENTS;i++) ctx->mon_tcp.clients[i]=-1;
        ESP_LOGI(TAG,"MON-TCP[%d]: listening on %s:%d",
                 (ctx==&g_ctx[1])?2:1, inet_ntoa(*(struct in_addr*)&a.sin_addr.s_addr), ctx->mon.ip.port);
        for(;;){
            struct sockaddr_in ca; socklen_t cl=sizeof(ca);
            int c = accept(s,(struct sockaddr*)&ca,&cl);
            if (c<0){
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            bool placed=false;
            for(int i=0;i<MAX_MON_TCP_CLIENTS;i++){
                if (ctx->mon_tcp.clients[i]<0){ ctx->mon_tcp.clients[i]=c; placed=true; break; }
            }
            if(!placed){ close(c); }
        }
    }
}

static void gate_tcp_client_task(void *pv){
    rt_ctx_t *ctx=(rt_ctx_t*)pv;
    ctx->gate_ip_cli.sock = -1;
    for(;;){
        if (!wait_ip_ready(ctx->gate.ifc, 500)) continue;
        int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (s<0){ vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        if (!bind_local_to_if(s, ctx->gate.ifc)){
            close(s);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        struct sockaddr_in to={0}; to.sin_family=AF_INET;
        to.sin_port=htons(ctx->gate.ip.port);
        to.sin_addr.s_addr=inet_addr(ctx->gate.ip.addr);
        ESP_LOGI(TAG,"GATE TCP-Client: connecting %s:%d", ctx->gate.ip.addr, ctx->gate.ip.port);
        if (connect(s,(struct sockaddr*)&to,sizeof(to))==0){
            ctx->gate_ip_cli.sock = s;
            uint8_t buf[1024];
            for(;;){
                ssize_t n = recv(s,buf,sizeof(buf),0);
                if (n<=0) break;
                int u = (&g_ctx[0]==ctx)?1:2;
                gate_send_uart(u, buf, n);
                char msg[128];
                snprintf(msg, sizeof(msg), "[%s][%s][%d][TCP]->[UART%d]",
                         ifc_name(ctx->gate.ifc), ctx->gate.ip.addr,
                         ctx->gate.ip.port, u);
                size_t msg_len = strlen(msg);
                uint8_t *pay = malloc(4 + msg_len + 1);
                if (pay) {
                    pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                    memcpy(pay+4, msg, msg_len+1);
                    send_uart_ws_data(u, pay, 4 + msg_len + 1);
                    free(pay);
                }
                mon_tee_matching(ctx, buf, n);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (ctx->gate_ip_cli.sock>=0){
            shutdown(ctx->gate_ip_cli.sock,SHUT_RDWR);
            close(ctx->gate_ip_cli.sock);
            ctx->gate_ip_cli.sock=-1;
        }
        close(s);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void gate_tcp_server_task(void *pv){
    rt_ctx_t *ctx=(rt_ctx_t*)pv;
    for(;;){
        if (!wait_ip_ready(ctx->gate.ifc, 500)) continue;
        int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (s<0){ vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        ctx->gate_ip_srv.listen_fd = s;
        int yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        struct sockaddr_in a={0};
        a.sin_family=AF_INET; a.sin_port=htons(ctx->gate.ip.port);
        a.sin_addr.s_addr = ip_of_if(ctx->gate.ifc);
        if (bind(s,(struct sockaddr*)&a,sizeof(a)) < 0) {
            ESP_LOGE(TAG,"GATE TCP-SRV: bind(%s:%d) failed, err=%d",
                     inet_ntoa(*(struct in_addr*)&a.sin_addr.s_addr), ctx->gate.ip.port, errno);
            close(s); ctx->gate_ip_srv.listen_fd = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (listen(s,1) < 0) {
            ESP_LOGE(TAG,"GATE TCP-SRV: listen(:%d) failed, err=%d", ctx->gate.ip.port, errno);
            close(s); ctx->gate_ip_srv.listen_fd = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        ESP_LOGI(TAG,"GATE TCP-Server: listening %s:%d",
                 inet_ntoa(*(struct in_addr*)&a.sin_addr.s_addr), ctx->gate.ip.port);
        for(;;){
            struct sockaddr_in ca; socklen_t cl=sizeof(ca);
            int c = accept(s,(struct sockaddr*)&ca,&cl);
            if (c<0){ vTaskDelay(pdMS_TO_TICKS(10)); continue; }
            ctx->gate_ip_srv.client_fd = c;
            ctx->gate_ip_srv.peer = ca;
            ESP_LOGI(TAG, "GATE TCP-Server: client connected from %s:%d",
                     inet_ntoa(ca.sin_addr), ntohs(ca.sin_port));
            char ip[16];
            strncpy(ip, inet_ntoa(ca.sin_addr), sizeof(ip)-1);
            ip[sizeof(ip)-1] = '\0';
            int rport = ntohs(ca.sin_port);
            uint8_t buf[1024];
            for(;;){
                ssize_t n = recv(c,buf,sizeof(buf),0);
                if (n<=0) break;
                int u = (&g_ctx[0]==ctx)?1:2;
                gate_send_uart(u, buf, n);
                char msg[128];
                snprintf(msg, sizeof(msg), "[%s][%s][%d][TCP]->[UART%d]",
                         ifc_name(ctx->gate.ifc), ip, rport, u);
                size_t msg_len = strlen(msg);
                uint8_t *pay = malloc(4 + msg_len + 1);
                if (pay) {
                    pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                    memcpy(pay+4, msg, msg_len+1);
                    send_uart_ws_data(u, pay, 4 + msg_len + 1);
                    free(pay);
                }
                mon_tee_matching(ctx, buf, n);
            }
            shutdown(c,SHUT_RDWR); close(c); ctx->gate_ip_srv.client_fd=-1;
            memset(&ctx->gate_ip_srv.peer, 0, sizeof(ctx->gate_ip_srv.peer));
        }
    }
}

static void gate_udp_client_task(void *pv){
    rt_ctx_t *ctx=(rt_ctx_t*)pv;
    for(;;){
        if (!wait_ip_ready(ctx->gate.ifc, 500)) continue;
        int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (s<0){ vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        if (!bind_local_to_if(s, ctx->gate.ifc)){
            close(s);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        ctx->gate_ip_cli.sock = s;
        struct timeval tv={.tv_sec=0,.tv_usec=0}; setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        for(;;){
            uint8_t buf[1500];
            struct sockaddr_in from; socklen_t fl=sizeof(from);
            ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fl);
            if (n>0){
                int u = (&g_ctx[0]==ctx)?1:2;
                gate_send_uart(u, buf, n);
                char ip[16];
                strncpy(ip, inet_ntoa(from.sin_addr), sizeof(ip)-1);
                ip[sizeof(ip)-1] = '\0';
                int rport = ntohs(from.sin_port);
                char msg[128];
                snprintf(msg, sizeof(msg), "[%s][%s][%d][UDP]->[UART%d]",
                         ifc_name(ctx->gate.ifc), ip, rport, u);
                size_t msg_len = strlen(msg);
                uint8_t *pay = malloc(4 + msg_len + 1);
                if (pay) {
                    pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                    memcpy(pay+4, msg, msg_len+1);
                    send_uart_ws_data(u, pay, 4 + msg_len + 1);
                    free(pay);
                }
                mon_tee_matching(ctx, buf, n);
            }else{
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

static void gate_udp_server_task(void *pv){
    rt_ctx_t *ctx = (rt_ctx_t*)pv;
    for(;;){
        if (!wait_ip_ready(ctx->gate.ifc, 500)) continue;
        ctx->gate_udp.udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        int s = ctx->gate_udp.udp_sock;
        if (s<0){ vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        int yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        struct sockaddr_in bindaddr={0};
        bindaddr.sin_family=AF_INET;
        bindaddr.sin_port=htons(ctx->gate.ip.port);
        bindaddr.sin_addr.s_addr = ip_of_if(ctx->gate.ifc);
        if (bind(s,(struct sockaddr*)&bindaddr,sizeof(bindaddr)) < 0){
            ESP_LOGE(TAG,"GATE-UDP: bind(%s:%d) failed, err=%d",
                     inet_ntoa(*(struct in_addr*)&bindaddr.sin_addr.s_addr), ctx->gate.ip.port, errno);
            close(s); ctx->gate_udp.udp_sock = -1;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        ESP_LOGI(TAG,"GATE UDP-Server: listening %s:%d",
                 inet_ntoa(*(struct in_addr*)&bindaddr.sin_addr.s_addr), ctx->gate.ip.port);
        ctx->gate_udp.udp_peer_cnt = 0;
        for(;;){
            uint8_t buf[1500];
            struct sockaddr_in from; socklen_t fl=sizeof(from);
            ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fl);
            if (n>0){
                bool known=false;
                for(size_t i=0;i<ctx->gate_udp.udp_peer_cnt;i++){
                    if (ctx->gate_udp.udp_peers[i].sin_addr.s_addr==from.sin_addr.s_addr &&
                        ctx->gate_udp.udp_peers[i].sin_port==from.sin_port){ known=true; break; }
                }
                if(!known && ctx->gate_udp.udp_peer_cnt<sizeof(ctx->gate_udp.udp_peers)/sizeof(ctx->gate_udp.udp_peers[0])){
                    ctx->gate_udp.udp_peers[ctx->gate_udp.udp_peer_cnt++] = from;
                }
                int u = (&g_ctx[0]==ctx)?1:2;
                gate_send_uart(u, buf, n);
                char ip[16];
                strncpy(ip, inet_ntoa(from.sin_addr), sizeof(ip)-1);
                ip[sizeof(ip)-1] = '\0';
                int rport = ntohs(from.sin_port);
                char msg[128];
                snprintf(msg, sizeof(msg), "[%s][%s][%d][UDP]->[UART%d]",
                         ifc_name(ctx->gate.ifc), ip, rport, u);
                size_t msg_len = strlen(msg);
                uint8_t *pay = malloc(4 + msg_len + 1);
                if (pay) {
                    pay[0]=0xFF; pay[1]='L'; pay[2]='O'; pay[3]='G';
                    memcpy(pay+4, msg, msg_len+1);
                    send_uart_ws_data(u, pay, 4 + msg_len + 1);
                    free(pay);
                }
                mon_tee_matching(ctx, buf, n);
            } else if (n<0) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
}

static void start_for_one(rt_ctx_t *ctx){
    if (is_ip_interface(ctx->gate.ifc) && ctx->gate.ip.tp!=TP_NONE){
        if (ctx->gate.ip.tp==TP_TCP){
            if (ctx->gate.ip.is_client){
                xTaskCreate(gate_tcp_client_task, "gate_tcp_cli", 4096, ctx, 8, NULL);
            }else{
                xTaskCreate(gate_tcp_server_task, "gate_tcp_srv", 4096, ctx, 8, NULL);
            }
        }else{
            if (ctx->gate.ip.is_client){
                xTaskCreate(gate_udp_client_task, "gate_udp_cli", 4096, ctx, 8, NULL);
            }else{
                ctx->gate_udp.udp_sock = -1; ctx->gate_udp.udp_peer_cnt = 0;
                xTaskCreate(gate_udp_server_task, "gate_udp_srv", 4096, ctx, 8, NULL);
            }
        }
    }
    if (ctx->gate.ifc==IF_MQTT && ctx->gate.rx_topic[0]){
        esp_mqtt_client_handle_t cli = get_mqtt_client_handle();
        if (cli) esp_mqtt_client_subscribe(cli, ctx->gate.rx_topic, 1);
    }
    if (is_ip_interface(ctx->mon.ifc)) {
        rt_ctx_t *other = (ctx == &g_ctx[0]) ? &g_ctx[1] : &g_ctx[0];
        if (ctx->mon.ip.tp==TP_UDP && !ctx->mon.ip.is_client) {
            bool duplicate = mon_cfg_equal(ctx, other) && other->mon_udp.udp_sock >= 0;
            if (!duplicate) {
                ctx->mon_udp.udp_sock = -1; ctx->mon_udp.udp_peer_cnt = 0;
                xTaskCreate(mon_udp_server_task, "mon_udp_srv", 4096, ctx, 7, NULL);
            } else {
                ESP_LOGW(TAG, "MON-UDP duplicate, reuse");
            }
        }
        if (ctx->mon.ip.tp==TP_TCP && !ctx->mon.ip.is_client) {
            bool duplicate = mon_cfg_equal(ctx, other) && other->mon_tcp.listen_fd >= 0;
            if (!duplicate) {
                for(int i=0;i<MAX_MON_TCP_CLIENTS;i++) ctx->mon_tcp.clients[i]=-1;
                ctx->mon_tcp.listen_fd=-1;
                xTaskCreate(mon_tcp_server_task, "mon_tcp_srv", 4096, ctx, 7, NULL);
            } else {
                ESP_LOGW(TAG, "MON-TCP duplicate, reuse");
            }
        }
    }
    if (ctx->mon.ifc==IF_MQTT && ctx->mon.rx_topic[0]){
        esp_mqtt_client_handle_t cli = get_mqtt_client_handle();
        if (cli) esp_mqtt_client_subscribe(cli, ctx->mon.rx_topic, 1);
    }
}

void router_start(void){
    memset(g_ctx,0,sizeof(g_ctx));
    for(int i=0;i<2;i++){
        const route_item_t *g = &global_routing_config.gateway[i];
        const route_item_t *m = &global_routing_config.monitor[i];
        fill_endpoint_from_route(g, &g_ctx[i].gate);
        fill_endpoint_from_route(m, &g_ctx[i].mon);
        g_ctx[i].gate_ip_cli.sock = -1;
        g_ctx[i].gate_ip_srv.listen_fd = -1;
        g_ctx[i].gate_ip_srv.client_fd = -1;
        memset(&g_ctx[i].gate_ip_srv.peer, 0, sizeof(g_ctx[i].gate_ip_srv.peer));
        g_ctx[i].gate_udp.udp_sock = -1;
        g_ctx[i].gate_udp.udp_peer_cnt = 0;
        start_for_one(&g_ctx[i]);
    }
    ESP_LOGI(TAG,"Router started: routes applied");
    extern_utility_start();
}
