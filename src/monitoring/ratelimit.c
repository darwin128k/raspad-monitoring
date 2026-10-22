#include <monitoring/ratelimit.h>

#include <time.h>

#define MONITORING_RATELIMIT_SLOTS 256u

typedef struct monitoring_ratelimit_slot
{
    uint32_t ip;
    time_t second;
    int32_t count;
} monitoring_ratelimit_slot_t;

static monitoring_ratelimit_slot_t g_slots[MONITORING_RATELIMIT_SLOTS];

authum_bool_t
monitoring_ratelimit_allow(uint32_t ip, int32_t per_second)
{
    monitoring_ratelimit_slot_t *slot;
    time_t now;

    if (per_second <= 0)
    {
        return authum_bool_true;
    }

    now = time(NULL);
    slot = &g_slots[(ip * 2654435761u) >> 24];
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
