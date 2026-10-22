#ifndef MONITORING_RATELIMIT_H
#define MONITORING_RATELIMIT_H

#include <authum/bool.h>

#include <stdint.h>

/*
 * Per-source-IP budget for answers we send ourselves (they bypass the Steam
 * challenge, so they are the amplification surface). Fixed table, 1-second
 * windows; a colliding IP just takes over the slot.
 */
authum_bool_t
monitoring_ratelimit_allow(uint32_t ip, int32_t per_second);

#endif /* MONITORING_RATELIMIT_H */
