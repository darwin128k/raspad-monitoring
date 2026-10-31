#ifndef MONITORING_REPLY_H
#define MONITORING_REPLY_H

#include <authum/byte.h>
#include <authum/char.h>
#include <authum/size.h>

/* Largest connectionless datagram we build (engine's MAX_ROUTEABLE_PACKET). */
#define MONITORING_REPLY_MAX 1400

/*
 * Inbound query kinds we care about. Everything else goes to Steam untouched.
 */
typedef enum monitoring_query
{
    monitoring_query_other = 0,
    monitoring_query_a2s_info,            /* "TSource Engine Query" with no usable challenge */
    monitoring_query_a2s_info_challenged, /* same, carrying a challenge — Steam's business */
    monitoring_query_legacy_info,         /* "info"    → SVC_Info(0), 'C' */
    monitoring_query_legacy_details,      /* "details" → SVC_Info(1), 'm' */
    monitoring_query_legacy_players,      /* "players" → SVC_PlayerInfo, 'D' */
    monitoring_query_legacy_rules,        /* "rules"   → SVC_RuleInfo, 'E' */
} monitoring_query_t;

monitoring_query_t
monitoring_reply_classify(const authum_u8_t *data, authum_usize_t len);

const authum_char_t *
monitoring_reply_query_name(monitoring_query_t query);

/*
 * Rewrite the engine's GoldSrc S2A_INFO_DETAILED ('m', what SVC_Info(1) sends)
 * into a Source-style A2S_INFO reply ('I'), the format Steam answers with.
 * EDF carries the game port (when non-zero, host byte order) and the game id.
 * Returns the reply length, or 0 when @p m is not a well-formed 'm' packet.
 */
/*
 * Copy an S2A_INFO ('C') or S2A_INFO_DETAILED ('m') reply with its leading
 * address string replaced by @p address ("ip:port"). Returns the new length,
 * or 0 when @p in is not 'C' / 'm' or the result does not fit.
 */
authum_usize_t
monitoring_reply_set_address(const authum_u8_t *in, authum_usize_t in_len,
                             const authum_char_t *address, authum_u8_t *out,
                             authum_usize_t out_size);

authum_usize_t
monitoring_reply_m_to_i(const authum_u8_t *m, authum_usize_t m_len, unsigned short appid,
                        const authum_char_t *version, unsigned short game_port, authum_u8_t *out,
                        authum_usize_t out_size);

#endif /* MONITORING_REPLY_H */
