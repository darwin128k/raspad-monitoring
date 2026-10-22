#include <monitoring/reply.h>

#include <string.h>
#include <strings.h>

static const char g_a2s_info_payload[] = "TSource Engine Query";

typedef struct monitoring_reader
{
    const uint8_t *p;
    const uint8_t *end;
    int ok;
} monitoring_reader_t;

typedef struct monitoring_writer
{
    uint8_t *p;
    uint8_t *end;
    int ok;
} monitoring_writer_t;

static const char *
reader_string(monitoring_reader_t *r)
{
    const uint8_t *nul;
    const char *s;

    if (!r->ok)
    {
        return "";
    }
    nul = memchr(r->p, 0, (size_t)(r->end - r->p));
    if (!nul)
    {
        r->ok = 0;
        return "";
    }
    s = (const char *)r->p;
    r->p = nul + 1;
    return s;
}

static uint8_t
reader_u8(monitoring_reader_t *r)
{
    if (!r->ok || r->p >= r->end)
    {
        r->ok = 0;
        return 0;
    }
    return *r->p++;
}

static void
reader_skip(monitoring_reader_t *r, size_t n)
{
    if (!r->ok || (size_t)(r->end - r->p) < n)
    {
        r->ok = 0;
        return;
    }
    r->p += n;
}

static void
writer_bytes(monitoring_writer_t *w, const void *data, size_t n)
{
    if (!w->ok || (size_t)(w->end - w->p) < n)
    {
        w->ok = 0;
        return;
    }
    memcpy(w->p, data, n);
    w->p += n;
}

static void
writer_u8(monitoring_writer_t *w, uint8_t v)
{
    writer_bytes(w, &v, 1);
}

static void
writer_u16(monitoring_writer_t *w, uint16_t v)
{
    uint8_t b[2] = {(uint8_t)(v & 0xff), (uint8_t)(v >> 8)};
    writer_bytes(w, b, sizeof b);
}

static void
writer_u64(monitoring_writer_t *w, uint64_t v)
{
    uint8_t b[8];
    int i;

    for (i = 0; i < 8; i++)
    {
        b[i] = (uint8_t)(v >> (8 * i));
    }
    writer_bytes(w, b, sizeof b);
}

static void
writer_string(monitoring_writer_t *w, const char *s)
{
    writer_bytes(w, s, strlen(s) + 1);
}

monitoring_query_t
monitoring_reply_classify(const uint8_t *data, size_t len)
{
    static const struct
    {
        const char *token;
        monitoring_query_t query;
    } legacy[] = {
        {"info", monitoring_query_legacy_info},
        {"details", monitoring_query_legacy_details},
        {"players", monitoring_query_legacy_players},
        {"rules", monitoring_query_legacy_rules},
    };
    const size_t a2s_len = sizeof g_a2s_info_payload - 1;
    const uint8_t *p;
    size_t n;
    size_t tok;
    size_t i;

    if (len < 5 || memcmp(data, "\xff\xff\xff\xff", 4) != 0)
    {
        return monitoring_query_other;
    }
    p = data + 4;
    n = len - 4;

    if (n >= a2s_len && memcmp(p, g_a2s_info_payload, a2s_len) == 0)
    {
        /* "TSource Engine Query\0" then an optional 4-byte challenge;
         * -1 is the "give me a challenge" placeholder, same as none. */
        if (n >= a2s_len + 1 + 4 && memcmp(p + a2s_len + 1, "\xff\xff\xff\xff", 4) != 0)
        {
            return monitoring_query_a2s_info_challenged;
        }
        return monitoring_query_a2s_info;
    }

    for (tok = 0; tok < n && p[tok] != ' ' && p[tok] != '\n' && p[tok] != '\0'; tok++)
    {
    }
    for (i = 0; i < sizeof legacy / sizeof legacy[0]; i++)
    {
        if (strlen(legacy[i].token) == tok &&
            strncasecmp((const char *)p, legacy[i].token, tok) == 0)
        {
            return legacy[i].query;
        }
    }
    return monitoring_query_other;
}

const char *
monitoring_reply_query_name(monitoring_query_t query)
{
    switch (query)
    {
    case monitoring_query_a2s_info:
        return "A2S_INFO";
    case monitoring_query_a2s_info_challenged:
        return "A2S_INFO+challenge";
    case monitoring_query_legacy_info:
        return "info";
    case monitoring_query_legacy_details:
        return "details";
    case monitoring_query_legacy_players:
        return "players";
    case monitoring_query_legacy_rules:
        return "rules";
    default:
        return "other";
    }
}

size_t
monitoring_reply_set_address(const uint8_t *in, size_t in_len, const char *address, uint8_t *out,
                             size_t out_size)
{
    monitoring_reader_t r;
    monitoring_writer_t w;

    if (in_len < 5 || memcmp(in, "\xff\xff\xff\xff", 4) != 0 || (in[4] != 'C' && in[4] != 'm'))
    {
        return 0;
    }
    r.p = in + 5;
    r.end = in + in_len;
    r.ok = 1;
    (void)reader_string(&r);
    if (!r.ok)
    {
        return 0;
    }

    w.p = out;
    w.end = out + out_size;
    w.ok = 1;
    writer_bytes(&w, in, 5);
    writer_string(&w, address);
    writer_bytes(&w, r.p, (size_t)(r.end - r.p));
    return w.ok ? (size_t)(w.p - out) : 0;
}

size_t
monitoring_reply_m_to_i(const uint8_t *m, size_t m_len, uint16_t appid, const char *version,
                        uint16_t game_port, uint8_t *out, size_t out_size)
{
    monitoring_reader_t r;
    monitoring_writer_t w;
    const char *name;
    const char *map;
    const char *folder;
    const char *game;
    uint8_t players;
    uint8_t max_players;
    uint8_t protocol;
    uint8_t type;
    uint8_t os;
    uint8_t password;
    uint8_t secure;
    uint8_t bots;
    uint8_t edf;

    if (m_len < 5 || memcmp(m, "\xff\xff\xff\xff", 4) != 0 || m[4] != 'm')
    {
        return 0;
    }

    /* S2A_INFO_DETAILED, as SVC_Info(1) writes it in build 10211. */
    r.p = m + 5;
    r.end = m + m_len;
    r.ok = 1;
    (void)reader_string(&r); /* net_local_adr */
    name = reader_string(&r);
    map = reader_string(&r);
    folder = reader_string(&r);
    game = reader_string(&r);
    players = reader_u8(&r);
    max_players = reader_u8(&r);
    protocol = reader_u8(&r);
    type = reader_u8(&r);
    os = reader_u8(&r);
    password = reader_u8(&r);
    if (reader_u8(&r))
    {
        /* gmodinfo: url_info, url_dl, "", version, size, svonly, cldll */
        (void)reader_string(&r);
        (void)reader_string(&r);
        (void)reader_string(&r);
        reader_skip(&r, 4 + 4 + 1 + 1);
    }
    secure = reader_u8(&r);
    bots = reader_u8(&r);
    if (!r.ok)
    {
        return 0;
    }

    w.p = out;
    w.end = out + out_size;
    w.ok = 1;
    writer_bytes(&w,
                 "\xff\xff\xff\xff"
                 "I",
                 5);
    writer_u8(&w, protocol);
    writer_string(&w, name);
    writer_string(&w, map);
    writer_string(&w, folder);
    writer_string(&w, game);
    writer_u16(&w, appid);
    writer_u8(&w, players);
    writer_u8(&w, max_players);
    writer_u8(&w, bots);
    writer_u8(&w, type);
    writer_u8(&w, os);
    writer_u8(&w, password);
    writer_u8(&w, secure);
    writer_string(&w, version);

    /* EDF: 0x80 game port, 0x01 game id (= appid for GoldSrc mods on Steam). */
    edf = 0x01;
    if (game_port)
    {
        edf |= 0x80;
    }
    writer_u8(&w, edf);
    if (game_port)
    {
        writer_u16(&w, game_port);
    }
    writer_u64(&w, (uint64_t)appid);

    return w.ok ? (size_t)(w.p - out) : 0;
}
