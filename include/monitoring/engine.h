#ifndef MONITORING_ENGINE_H
#define MONITORING_ENGINE_H

#include <authum/byte.h>

/*
 * netadr_t from the engine's netadr.h (20 bytes, passed by value).
 * ip[] and port are in network byte order.
 */
typedef struct monitoring_netadr
{
    authum_i32_t type;
    authum_u8_t ip[4];
    authum_u8_t ipx[10];
    /* The engine's 16-bit port; the Authum facade has no 16-bit type. */
    unsigned short port;
} monitoring_netadr_t;

_Static_assert(sizeof(monitoring_netadr_t) == 20, "netadr_t must stay 20 bytes");

/* netsrc_t: NS_CLIENT = 0, NS_SERVER = 1. */
enum
{
    MONITORING_NS_SERVER = 1
};

#endif /* MONITORING_ENGINE_H */
