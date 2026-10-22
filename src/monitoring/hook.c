/*
 * Where server queries go in engine_i486.so build 10211 (objdump of SV_ReadPackets):
 *
 *   NET_GetPacket
 *     -> CheckIP(net_from)                          max_queries_* budget
 *     -> Steam_HandleIncomingPacket(data, len, ip, port)
 *          SteamGameServer()->HandleIncomingPacket  A2S_INFO / PLAYER / RULES + challenge
 *     -> SV_ConnectionlessPacket                    ping, getchallenge, connect, rcon
 *
 * The return value of Steam_HandleIncomingPacket is ignored and
 * SV_ConnectionlessPacket runs regardless. Since Steam started answering a
 * bare A2S_INFO with an 'A' challenge, monitorings that never send the second
 * query see the server as offline; the old text queries (info / details /
 * players / rules) lost their handlers in SV_ConnectionlessPacket altogether,
 * though SVC_Info / SVC_PlayerInfo / SVC_RuleInfo are still in the binary.
 *
 * So: for those queries we answer through the engine's own SVC_* handlers
 * (they read net_from) and keep the packet from Steam. SVC_Info's reply is
 * captured instead of sent (a second detour on NET_SendPacket, armed only for
 * that call) so we can turn 'm' into 'I' and put the real address into 'C' /
 * 'm'. Everything else goes to Steam unchanged.
 */

#include <monitoring/hook.h>

#include <monitoring/config.h>
#include <monitoring/engine.h>
#include <monitoring/host.h>
#include <monitoring/allowlist.h>
#include <monitoring/ratelimit.h>
#include <monitoring/reply.h>

#include <authum/detour.h>
#include <authum/logger.h>
#include <authum/symbol.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*steam_handle_incoming_packet_fn)(const void *data, int len, uint32_t ip,
                                               uint32_t port);
typedef void (*net_send_packet_fn)(int sock, int len, const void *data, monitoring_netadr_t to);
typedef void (*svc_info_fn)(int detailed);
typedef void (*svc_handler_fn)(void);
typedef char *(*cvar_variable_string_fn)(const char *);

static authum_detour_t g_incoming_detour;
static authum_detour_t g_send_detour; /* attached once, patched only while capturing */

static steam_handle_incoming_packet_fn g_steam_handle_incoming_packet;
static net_send_packet_fn g_net_send_packet;
static svc_info_fn g_svc_info;
static svc_handler_fn g_svc_player_info;
static svc_handler_fn g_svc_rule_info;
static cvar_variable_string_fn g_cvar_variable_string;
static monitoring_netadr_t *g_net_from;

static uint8_t g_capture[MONITORING_REPLY_MAX];
static size_t g_capture_len;

static authum_ptr
monitoring_hook_resolve(const char *name)
{
    authum_ptr address = authum_symbol_find_engine(name, authum_null);

    authum_logger_info(&g_monitoring_logger, "engine resolve: %s @ %p", name, address);
    return address;
}

/* Stands in for NET_SendPacket while SVC_Info runs: keep the bytes, send nothing. */
static void
monitoring_hook_capture_send(int sock, int len, const void *data, monitoring_netadr_t to)
{
    (void)sock;
    (void)to;
    if (len > 0 && (size_t)len <= sizeof g_capture)
    {
        memcpy(g_capture, data, (size_t)len);
        g_capture_len = (size_t)len;
    }
}

/* SVC_Info(0) → 'C', SVC_Info(1) → 'm', into g_capture. 0 when SVC_Info bailed. */
static size_t
monitoring_hook_capture_info(int detailed)
{
    g_capture_len = 0;
    authum_detour_resume(&g_send_detour);
    g_svc_info(detailed);
    authum_detour_suspend(&g_send_detour);
    return g_capture_len;
}

/* Port the engine listens on: same precedence as NET_OpenIP. 0 = unknown. */
static uint16_t
monitoring_hook_server_port(void)
{
    static const char *const names[] = {"ip_hostport", "hostport", "port"};
    const char *value;
    int port;
    size_t i;

    if (!g_cvar_variable_string)
    {
        return 0u;
    }
    for (i = 0; i < sizeof names / sizeof names[0]; i++)
    {
        value = g_cvar_variable_string(names[i]);
        port = value ? atoi(value) : 0;
        if (port > 0 && port < 65536)
        {
            return (uint16_t)port;
        }
    }
    return 0u;
}

/* "ip:port" for 'C' / 'm': public_ip, else the `ip` cvar; 0 = keep the engine's. */
static int
monitoring_hook_public_address(char *buf, size_t buf_size)
{
    const char *ip = g_monitoring_config.public_ip;
    uint16_t port = monitoring_hook_server_port();

    if (!ip[0] && g_cvar_variable_string)
    {
        ip = g_cvar_variable_string("ip");
    }
    if (!ip || !ip[0] || strcmp(ip, "localhost") == 0 || strcmp(ip, "0.0.0.0") == 0 || !port)
    {
        return 0;
    }
    snprintf(buf, buf_size, "%s:%u", ip, (unsigned)port);
    return 1;
}

static void
monitoring_hook_send(const uint8_t *data, size_t len)
{
    g_net_send_packet(MONITORING_NS_SERVER, (int)len, data, *g_net_from);
}

/* Send a captured 'C' / 'm', with the public address when we know it. */
static void
monitoring_hook_send_info(const uint8_t *data, size_t len)
{
    uint8_t reply[MONITORING_REPLY_MAX];
    char address[96];
    size_t reply_len = 0;

    if (monitoring_hook_public_address(address, sizeof address))
    {
        reply_len = monitoring_reply_set_address(data, len, address, reply, sizeof reply);
    }
    if (reply_len)
    {
        monitoring_hook_send(reply, reply_len);
    }
    else
    {
        monitoring_hook_send(data, len);
    }
}

/* Returns 1 when we answered (the packet must not reach Steam). */
static int
monitoring_hook_answer_a2s_info(void)
{
    uint8_t reply[MONITORING_REPLY_MAX];
    size_t reply_len;
    monitoring_info_answer_t answer = g_monitoring_config.info_answer;

    if (!monitoring_hook_capture_info(1))
    {
        return 0; /* SVC_Info bailed (no map / single player) — let Steam decide */
    }

    if (answer == monitoring_info_answer_new || answer == monitoring_info_answer_hybrid)
    {
        reply_len = monitoring_reply_m_to_i(
            g_capture, g_capture_len, (uint16_t)g_monitoring_config.appid,
            g_monitoring_config.version, monitoring_hook_server_port(), reply, sizeof reply);
        if (!reply_len)
        {
            authum_logger_warning(&g_monitoring_logger,
                                  "SVC_Info reply (%u bytes) did not parse — left to Steam",
                                  (unsigned)g_capture_len);
            return 0;
        }
        monitoring_hook_send(reply, reply_len);
    }
    if (answer == monitoring_info_answer_old || answer == monitoring_info_answer_hybrid)
    {
        monitoring_hook_send_info(g_capture, g_capture_len);
    }
    return 1;
}

static authum_bool_t
monitoring_hook_enabled(monitoring_query_t query)
{
    switch (query)
    {
    case monitoring_query_a2s_info:
        return g_monitoring_config.info_answer != monitoring_info_answer_steam;
    case monitoring_query_legacy_info:
        return g_monitoring_config.legacy_info;
    case monitoring_query_legacy_details:
        return g_monitoring_config.legacy_details;
    case monitoring_query_legacy_players:
        return g_monitoring_config.legacy_players && g_svc_player_info;
    case monitoring_query_legacy_rules:
        return g_monitoring_config.legacy_rules && g_svc_rule_info;
    default:
        return authum_bool_false;
    }
}

static int
monitoring_hook_answer(monitoring_query_t query, uint32_t ip)
{
    if (!monitoring_hook_enabled(query))
    {
        return 0;
    }

    if (g_monitoring_config.info_answer_scope == monitoring_info_answer_scope_list &&
        !monitoring_allowlist_contains(((uint32_t)g_net_from->ip[0] << 24) |
                                       ((uint32_t)g_net_from->ip[1] << 16) |
                                       ((uint32_t)g_net_from->ip[2] << 8) | g_net_from->ip[3]))
    {
        if (g_monitoring_config.debug)
        {
            authum_logger_info(
                &g_monitoring_logger, "%s from %u.%u.%u.%u not in %s — left to Steam",
                monitoring_reply_query_name(query), g_net_from->ip[0], g_net_from->ip[1],
                g_net_from->ip[2], g_net_from->ip[3], g_monitoring_config.monitoring_list);
        }
        return 0;
    }

    if (!monitoring_ratelimit_allow(ip, g_monitoring_config.ratelimit_per_ip))
    {
        if (g_monitoring_config.debug)
        {
            authum_logger_info(&g_monitoring_logger, "%s from %u.%u.%u.%u rate-limited",
                               monitoring_reply_query_name(query), g_net_from->ip[0],
                               g_net_from->ip[1], g_net_from->ip[2], g_net_from->ip[3]);
        }
        return 0;
    }

    switch (query)
    {
    case monitoring_query_a2s_info:
        if (!monitoring_hook_answer_a2s_info())
        {
            return 0;
        }
        break;
    case monitoring_query_legacy_info:
    case monitoring_query_legacy_details:
        if (monitoring_hook_capture_info(query == monitoring_query_legacy_details))
        {
            monitoring_hook_send_info(g_capture, g_capture_len);
        }
        break;
    case monitoring_query_legacy_players:
        g_svc_player_info();
        break;
    case monitoring_query_legacy_rules:
        g_svc_rule_info();
        break;
    default:
        return 0;
    }

    if (g_monitoring_config.debug)
    {
        authum_logger_info(&g_monitoring_logger, "%s from %u.%u.%u.%u:%u answered (%s)",
                           monitoring_reply_query_name(query), g_net_from->ip[0], g_net_from->ip[1],
                           g_net_from->ip[2], g_net_from->ip[3],
                           (unsigned)((g_net_from->port >> 8) | ((g_net_from->port & 0xff) << 8)),
                           query == monitoring_query_a2s_info
                               ? monitoring_info_answer_name(g_monitoring_config.info_answer)
                               : "engine");
    }
    return 1;
}

static int
monitoring_hook_steam_handle_incoming_packet(const void *data, int len, uint32_t ip, uint32_t port)
{
    monitoring_query_t query = monitoring_query_other;
    int result;

    if (data && len > 0)
    {
        query = monitoring_reply_classify((const uint8_t *)data, (size_t)len);
    }
    if (monitoring_hook_answer(query, ip))
    {
        return 1;
    }

    authum_detour_suspend(&g_incoming_detour);
    result = g_steam_handle_incoming_packet(data, len, ip, port);
    authum_detour_resume(&g_incoming_detour);
    return result;
}

int
monitoring_hook_install(void)
{
    char address[96];

    if (g_incoming_detour.attached)
    {
        return 1;
    }

    g_steam_handle_incoming_packet =
        (steam_handle_incoming_packet_fn)monitoring_hook_resolve("Steam_HandleIncomingPacket");
    g_net_send_packet = (net_send_packet_fn)monitoring_hook_resolve("NET_SendPacket");
    g_svc_info = (svc_info_fn)monitoring_hook_resolve("SVC_Info");
    g_net_from = (monitoring_netadr_t *)monitoring_hook_resolve("net_from");
    if (!g_steam_handle_incoming_packet || !g_net_send_packet || !g_svc_info || !g_net_from)
    {
        authum_logger_error(&g_monitoring_logger, "required engine symbols missing");
        return 0;
    }
    g_svc_player_info = (svc_handler_fn)monitoring_hook_resolve("SVC_PlayerInfo");
    g_svc_rule_info = (svc_handler_fn)monitoring_hook_resolve("SVC_RuleInfo");
    g_cvar_variable_string =
        (cvar_variable_string_fn)monitoring_hook_resolve("Cvar_VariableString");

    /* NET_SendPacket stays original except inside monitoring_hook_capture_info. */
    if (!authum_detour_attach(&g_send_detour, (authum_ptr)g_net_send_packet,
                              (authum_ptr)&monitoring_hook_capture_send))
    {
        authum_logger_error(&g_monitoring_logger, "detour on NET_SendPacket FAILED");
        return 0;
    }
    authum_detour_suspend(&g_send_detour);

    if (!authum_detour_attach(&g_incoming_detour, (authum_ptr)g_steam_handle_incoming_packet,
                              (authum_ptr)&monitoring_hook_steam_handle_incoming_packet))
    {
        authum_detour_detach(&g_send_detour);
        authum_logger_error(&g_monitoring_logger, "detour on Steam_HandleIncomingPacket FAILED");
        return 0;
    }
    authum_logger_info(&g_monitoring_logger, "hook installed on Steam_HandleIncomingPacket @ %p",
                       (void *)g_steam_handle_incoming_packet);
    if (monitoring_hook_public_address(address, sizeof address))
    {
        authum_logger_info(&g_monitoring_logger, "'C' / 'm' address: %s", address);
    }
    else
    {
        authum_logger_info(&g_monitoring_logger,
                           "'C' / 'm' address: engine's own (set public_ip in monitoring.cfg)");
    }
    return 1;
}

void
monitoring_hook_remove(void)
{
    if (!g_incoming_detour.attached)
    {
        return;
    }
    authum_detour_detach(&g_incoming_detour);
    authum_detour_detach(&g_send_detour);
    authum_logger_info(&g_monitoring_logger, "hook removed, original bytes restored");
}
