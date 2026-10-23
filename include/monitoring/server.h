#ifndef MONITORING_SERVER_H
#define MONITORING_SERVER_H

#include <authum/bool.h>
#include <authum/byte.h>

#include <stddef.h>
#include <stdint.h>

/*
 * What the 'I' / 'C' / 'm' replies need that the engine does not hand over,
 * taken from the same places Steam takes them:
 *   appid    — steam_appid.txt in the HLDS root (next to the game dir)
 *   version  — PatchVersion in <gamedir>/steam.inf, plus "/Stdio" (the
 *              engine's filesystem module, appended by Steam the same way)
 *   address  — the one external IPv4 on this machine (<authum/net.h>), for
 *              HLDS bound to every interface (0.0.0.0)
 * Values set in monitoring.cfg win; these only fill what is left empty.
 */

/* Fill the empty appid / version in g_monitoring_config, detect the public
 * address, and log where each value came from. Call after the cfg is loaded. */
void
monitoring_server_resolve(void);

/* Detected public IPv4 ("" when none or ambiguous). */
const char *
monitoring_server_detected_ip(void);

/* steam_appid.txt → appid, 0 when missing or not a number in 1..65535. */
authum_i32_t
monitoring_server_read_appid(const char *path);

/* steam.inf → "<PatchVersion>/Stdio" into @p out. False when missing. */
authum_bool_t
monitoring_server_read_version(const char *path, char *out, size_t out_size);

#endif /* MONITORING_SERVER_H */
