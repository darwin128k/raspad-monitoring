#ifndef MONITORING_HOOK_H
#define MONITORING_HOOK_H

/*
 * Detour on the engine's Steam_HandleIncomingPacket — every connectionless
 * packet passes it (after CheckIP / max_queries) before SV_ConnectionlessPacket.
 * Returns 1 on success.
 */
int
monitoring_hook_install(void);

void
monitoring_hook_remove(void);

#endif /* MONITORING_HOOK_H */
