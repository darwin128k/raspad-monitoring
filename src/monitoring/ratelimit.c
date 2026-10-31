#include <monitoring/ratelimit.h>

#include <authum/time.h>

#define MONITORING_RATELIMIT_SLOTS 256u

typedef struct monitoring_ratelimit_slot
{
    authum_u32_t ip;
    authum_i64_t second;
    authum_i32_t count;
} monitoring_ratelimit_slot_t;

static monitoring_ratelimit_slot_t g_slots[MONITORING_RATELIMIT_SLOTS];

authum_bool_t
monitoring_ratelimit_allow(authum_u32_t ip, authum_i32_t per_second)
{
    if (per_second <= 0)
    {
        return authum_bool_true;
    }

    const authum_i64_t now = authum_time_now_seconds();
    monitoring_ratelimit_slot_t *slot = &g_slots[(ip * 2654435761u) >> 24];

    if (slot->ip != ip || slot->second != now)
    {
        slot->ip = ip;
        slot->second = now;
        slot->count = 0;
    }
    if (slot->count >= per_second)
    {
        return authum_bool_false;
    }
    slot->count++;
    return authum_bool_true;
}
