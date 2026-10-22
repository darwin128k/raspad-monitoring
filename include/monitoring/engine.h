#ifndef MONITORING_ENGINE_H
#define MONITORING_ENGINE_H

#include <stdint.h>

/*
 * netadr_t from the engine's netadr.h (20 bytes, passed by value).
 * ip[] and port are in network byte order.
 */
typedef struct monitoring_netadr
{
    int32_t type;
    uint8_t ip[4];
    uint8_t ipx[10];
    uint16_t port;
} monitoring_netadr_t;

_Static_assert(sizeof(monitoring_netadr_t) == 20, "netadr_t must stay 20 bytes");

/* netsrc_t: NS_CLIENT = 0, NS_SERVER = 1. */
enum
{
    MONITORING_NS_SERVER = 1
};

#endif /* MONITORING_ENGINE_H */
