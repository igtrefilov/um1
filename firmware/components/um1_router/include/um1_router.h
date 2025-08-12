#ifndef UM1_ROUTER_H
#define UM1_ROUTER_H


#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Minimal public interface of the router.  The full implementation lives in
 * um1_router.c and follows a precompiled index approach.  For the existing
 * code base we expose only the pieces that are currently required by other
 * components.  The new router can be fed with rich metadata (rx_meta_t) or via
 * the legacy string based API.
 */

typedef enum {
    IF_UART1,
    IF_UART2,
    IF_LAN,
    IF_AP,
    IF_STA,
} iface_type_t;

typedef enum {
    PROTO_NONE = 0,
    PROTO_TCP  = 1,
    PROTO_UDP  = 2,
    PROTO_MQTT = 3,
} proto_t;

typedef struct {
    uint16_t    src_id;   /* endpoint identifier */
    proto_t     proto;    /* protocol of received data */
    uint32_t    ip;       /* source IP (network order) */
    uint16_t    port;     /* source port */
    const char *topic;    /* MQTT topic, if any */
} rx_meta_t;

void router_init(void);
void route_data(const char *src_if, const uint8_t *data, size_t len);
void route_data_meta(const rx_meta_t *meta, const uint8_t *data, size_t len);

#endif // UM1_ROUTER_H
