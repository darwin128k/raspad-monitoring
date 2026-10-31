#include <monitoring/reply.h>

#include <authum/cstr.h>
#include <authum/memory.h>

static const authum_char_t g_a2s_info_payload[] = "TSource Engine Query";

typedef struct monitoring_reader
{
    const authum_u8_t *p;
    const authum_u8_t *end;
    int ok;
} monitoring_reader_t;

typedef struct monitoring_writer
{
    authum_u8_t *p;
    authum_u8_t *end;
    int ok;
} monitoring_writer_t;

static const authum_char_t *
reader_string(monitoring_reader_t *r)
{
    const authum_u8_t *nul;
    const authum_char_t *s;
    authum_u8_t zero = 0;

    if (!r->ok)
    {
        return "";
    }
    nul = authum_memory_find(r->p, (authum_usize_t)(r->end - r->p), &zero, 1);
    if (!nul)
    {
        r->ok = 0;
        return "";
    }
    s = (const authum_char_t *)r->p;
    r->p = nul + 1;
    return s;
}

static authum_u8_t
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
reader_skip(monitoring_reader_t *r, authum_usize_t n)
{
    if (!r->ok || (authum_usize_t)(r->end - r->p) < n)
    {
        r->ok = 0;
        return;
    }
    r->p += n;
}

static void
writer_bytes(monitoring_writer_t *w, const void *data, authum_usize_t n)
{
    if (!w->ok || (authum_usize_t)(w->end - w->p) < n)
    {
        w->ok = 0;
        return;
    }
    authum_memory_copy(w->p, data, n);
    w->p += n;
}

static void
writer_u8(monitoring_writer_t *w, authum_u8_t v)
{
    writer_bytes(w, &v, 1);
}

static void
writer_u16(monitoring_writer_t *w, unsigned short v)
{
    authum_u8_t b[2] = {(authum_u8_t)(v & 0xff), (authum_u8_t)(v >> 8)};

    writer_bytes(w, b, sizeof b);
}

static void
writer_u64(monitoring_writer_t *w, authum_u64_t v)
{
    authum_u8_t b[8];

    for (int i = 0; i < 8; i++)
    {
        b[i] = (authum_u8_t)(v >> (8 * i));
    }
    writer_bytes(w, b, sizeof b);
}

static void
writer_string(monitoring_writer_t *w, const authum_char_t *s)
{
    writer_bytes(w, s, authum_cstr_len(s) + 1);
}

/* The length of the token at the start of @p p, up to a space, LF or NUL. */
static authum_usize_t
monitoring_reply_token_length(const authum_u8_t *p, authum_usize_t size)
{
    authum_usize_t tok = 0;

    while (tok < size && p[tok] != ' ' && p[tok] != '\n' && p[tok] != '\0')
    {
        tok++;
    }
    return tok;
}

monitoring_query_t
monitoring_reply_classify(const authum_u8_t *data, authum_usize_t len)
{
    if (len < 5 || !authum_memory_is_equal(data, "\xff\xff\xff\xff", 4))
    {
        return monitoring_query_other;
    }

    const authum_u8_t *p = data + 4;
    const authum_usize_t n = len - 4;
    const authum_usize_t a2s_len = sizeof g_a2s_info_payload - 1;

    if (n >= a2s_len && authum_memory_is_equal(p, g_a2s_info_payload, a2s_len))
    {
        /* "TSource Engine Query\0" then an optional 4-byte challenge;
         * -1 is the "give me a challenge" placeholder, same as none. */
        if (n >= a2s_len + 1 + 4 && !authum_memory_is_equal(p + a2s_len + 1, "\xff\xff\xff\xff", 4))
        {
            return monitoring_query_a2s_info_challenged;
        }
        return monitoring_query_a2s_info;
    }

    static const struct
    {
        const authum_char_t *token;
        monitoring_query_t query;
    } legacy[] = {
        {"info", monitoring_query_legacy_info},
        {"details", monitoring_query_legacy_details},
        {"players", monitoring_query_legacy_players},
        {"rules", monitoring_query_legacy_rules},
    };

    const authum_usize_t tok = monitoring_reply_token_length(p, n);

    for (authum_usize_t i = 0; i < sizeof legacy / sizeof legacy[0]; i++)
    {
        if (authum_cstr_len(legacy[i].token) == tok &&
            authum_cstr_is_equal_n((const authum_char_t *)p, legacy[i].token, tok,
                                   authum_bool_true))
        {
            return legacy[i].query;
        }
    }
    return monitoring_query_other;
}

const authum_char_t *
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

authum_usize_t
monitoring_reply_set_address(const authum_u8_t *in, authum_usize_t in_len,
                             const authum_char_t *address, authum_u8_t *out,
                             authum_usize_t out_size)
{
    if (in_len < 5 || !authum_memory_is_equal(in, "\xff\xff\xff\xff", 4) ||
        (in[4] != 'C' && in[4] != 'm'))
    {
        return 0;
    }

    monitoring_reader_t r = {
        .p = in + 5,
        .end = in + in_len,
        .ok = 1,
    };

    (void)reader_string(&r);
    if (!r.ok)
    {
        return 0;
    }

    monitoring_writer_t w = {
        .p = out,
        .end = out + out_size,
        .ok = 1,
    };

    writer_bytes(&w, in, 5);
    writer_string(&w, address);
    writer_bytes(&w, r.p, (authum_usize_t)(r.end - r.p));
    return w.ok ? (authum_usize_t)(w.p - out) : 0;
}

authum_usize_t
monitoring_reply_m_to_i(const authum_u8_t *m, authum_usize_t m_len, unsigned short appid,
                        const authum_char_t *version, unsigned short game_port, authum_u8_t *out,
                        authum_usize_t out_size)
{
    if (m_len < 5 || !authum_memory_is_equal(m, "\xff\xff\xff\xff", 4) || m[4] != 'm')
    {
        return 0;
    }

    /* S2A_INFO_DETAILED, as SVC_Info(1) writes it in build 10211. */
    monitoring_reader_t r = {
        .p = m + 5,
        .end = m + m_len,
        .ok = 1,
    };

    (void)reader_string(&r); /* net_local_adr */
    const authum_char_t *name = reader_string(&r);
    const authum_char_t *map = reader_string(&r);
    const authum_char_t *folder = reader_string(&r);
    const authum_char_t *game = reader_string(&r);
    const authum_u8_t players = reader_u8(&r);
    const authum_u8_t max_players = reader_u8(&r);
    const authum_u8_t protocol = reader_u8(&r);
    const authum_u8_t type = reader_u8(&r);
    const authum_u8_t os = reader_u8(&r);
    const authum_u8_t password = reader_u8(&r);

    if (reader_u8(&r))
    {
        /* gmodinfo: url_info, url_dl, "", version, size, svonly, cldll */
        (void)reader_string(&r);
        (void)reader_string(&r);
        (void)reader_string(&r);
        reader_skip(&r, 4 + 4 + 1 + 1);
    }

    const authum_u8_t secure = reader_u8(&r);
    const authum_u8_t bots = reader_u8(&r);

    if (!r.ok)
    {
        return 0;
    }

    monitoring_writer_t w = {
        .p = out,
        .end = out + out_size,
        .ok = 1,
    };

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
    authum_u8_t edf = 0x01;

    if (game_port)
    {
        edf |= 0x80;
    }
    writer_u8(&w, edf);
    if (game_port)
    {
        writer_u16(&w, game_port);
    }
    writer_u64(&w, (authum_u64_t)appid);

    return w.ok ? (authum_usize_t)(w.p - out) : 0;
}
