#ifndef MONITORING_CONFIG_H
#define MONITORING_CONFIG_H

#include <authum/bool.h>
#include <authum/byte.h>
#include <authum/char.h>
#include <authum/logger/file/split.h>
#include <authum/logger/sink.h>

/*
 * What to send back for an A2S_INFO ("TSource Engine Query") that carries no
 * valid challenge. Same idea as dproto/reunion ServerInfoAnswerType.
 */
enum monitoring_info_answer
{
    monitoring_info_answer_steam = 0,  /* untouched: Steam replies with an 'A' challenge */
    monitoring_info_answer_new = 1,    /* 'I' right away */
    monitoring_info_answer_old = 2,    /* GoldSrc 'm' only */
    monitoring_info_answer_hybrid = 3, /* 'I' followed by 'm' */
};
typedef enum monitoring_info_answer monitoring_info_answer_t;

/*
 * Who gets answers that skip the Steam challenge (A2S_INFO and the legacy
 * text queries). Everyone else is left to Steam: 'A' challenge for A2S_INFO,
 * no answer for info / details / players / rules.
 */
enum monitoring_info_answer_scope
{
    monitoring_info_answer_scope_all = 0,  /* any source, rate-limited per IP */
    monitoring_info_answer_scope_list = 1, /* only sources in monitorings.ini */
};
typedef enum monitoring_info_answer_scope monitoring_info_answer_scope_t;

struct monitoring_config
{
    monitoring_info_answer_t info_answer;
    monitoring_info_answer_scope_t info_answer_scope;
    /* Known monitoring pollers, leaf under the module dir (see allowlist.h). */
    authum_char_t monitorings[64];
    /* Text queries Steam dropped, each on its own: rules is the big one
     * (several KB for a 10-byte query), so it ships off. */
    authum_bool_t legacy_info;
    authum_bool_t legacy_details;
    authum_bool_t legacy_players;
    authum_bool_t legacy_rules;
    /* Address written into 'C' / 'm' instead of the engine's net_local_adr
     * (0.0.0.0 when HLDS listens on every interface). Empty = `ip` cvar. */
    authum_char_t public_ip[64];
    /* Fields of the 'I' reply the GoldSrc 'm' reply does not carry. */
    authum_i32_t appid;
    authum_char_t version[32];
    /* Direct answers per source IP per second; 0 = unlimited. */
    authum_i32_t ratelimit_per_ip;
    authum_bool_t debug;

    authum_logger_sink_t logging_sinks;
    authum_logger_file_split_t logging_split;
};
typedef struct monitoring_config monitoring_config_t;

extern monitoring_config_t g_monitoring_config;

void
monitoring_config_load(void);

const authum_char_t *
monitoring_info_answer_name(monitoring_info_answer_t answer);

const authum_char_t *
monitoring_info_answer_scope_name(monitoring_info_answer_scope_t scope);

#endif /* MONITORING_CONFIG_H */
