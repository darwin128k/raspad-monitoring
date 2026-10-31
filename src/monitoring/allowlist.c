#include <monitoring/allowlist.h>

#include <monitoring/host.h>

#include <authum/cstr.h>
#include <authum/file.h>
#include <authum/host.h>
#include <authum/logger.h>
#include <authum/memory.h>
#include <authum/null.h>
#include <authum/path.h>

static monitoring_allowlist_entry_t g_entries[MONITORING_ALLOWLIST_MAX];
static authum_usize_t g_count;

static int
monitoring_allowlist_parse_octet(const authum_char_t **p, authum_u32_t *out)
{
    unsigned value = 0;
    int digits = 0;

    while (authum_char_is_digit(**p))
    {
        value = value * 10u + (unsigned)(**p - '0');
        (*p)++;
        if (++digits > 3 || value > 255u)
        {
            return 0;
        }
    }
    *out = value;
    return digits > 0;
}

int
monitoring_allowlist_parse_line(const authum_char_t *line, monitoring_allowlist_entry_t *out)
{
    const authum_char_t *p = line;
    authum_u32_t ip = 0;
    unsigned bits = 32;

    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';')
    {
        return 0;
    }

    for (int i = 0; i < 4; i++)
    {
        authum_u32_t octet = 0;

        if (!monitoring_allowlist_parse_octet(&p, &octet) || (i < 3 && *p++ != '.'))
        {
            return -1;
        }
        ip = (ip << 8) | octet;
    }
    if (*p == '/')
    {
        authum_u32_t value = 0;

        p++;
        if (!monitoring_allowlist_parse_octet(&p, &value) || value > 32u)
        {
            return -1;
        }
        bits = (unsigned)value;
    }
    if (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '#' && *p != ';')
    {
        return -1;
    }

    out->mask = bits ? 0xffffffffu << (32u - bits) : 0u;
    out->net = ip & out->mask;
    out->bits = (authum_u8_t)bits;
    return 1;
}

authum_bool_t
monitoring_allowlist_match(const monitoring_allowlist_entry_t *entries, authum_usize_t count,
                           authum_u32_t ip)
{
    for (authum_usize_t i = 0; i < count; i++)
    {
        if ((ip & entries[i].mask) == entries[i].net)
        {
            return authum_bool_true;
        }
    }
    return authum_bool_false;
}

int
monitoring_allowlist_load(const authum_char_t *leaf)
{
    static authum_char_t text[MONITORING_ALLOWLIST_TEXT_MAX];
    authum_usize_t size = 0;

    g_count = 0;

    /* modules/monitoring/configs/monitorings.ini */
    {
        authum_path_t path;
        authum_file_t file;

        if (!authum_host_join_config_path(&g_monitoring_host, leaf, &path))
        {
            return -1;
        }
        authum_file_init(&file);
        if (!authum_file_open(&file, &path, authum_file_mode_read))
        {
            return -1;
        }
        if (!authum_file_read_all(&file, text, sizeof text, &size))
        {
            authum_file_close(&file);
            authum_logger_warning(&g_monitoring_logger, "%s: over %d bytes — not loaded", leaf,
                                  (int)sizeof text);
            return -1;
        }
        authum_file_close(&file);
    }

    /* The text is split here, not by the file API: it reads bytes, not lines. */
    {
        authum_char_t *line = text;
        authum_char_t *end = text + size;
        int number = 0;

        while (line < end)
        {
            authum_char_t buffer[256];
            authum_byte_t newline_byte = (authum_byte_t)'\n';
            const authum_byte_t *newline;
            authum_usize_t length;
            int parsed;
            monitoring_allowlist_entry_t entry;

            newline = authum_memory_find(line, (authum_usize_t)(end - line), &newline_byte, 1);
            length = newline ? (authum_usize_t)((const authum_char_t *)newline - line)
                             : (authum_usize_t)(end - line);
            if (length >= sizeof buffer)
            {
                length = sizeof buffer - 1;
            }
            authum_memory_copy(buffer, line, length);
            buffer[length] = '\0';
            if (length > 0 && buffer[length - 1] == '\r')
            {
                buffer[length - 1] = '\0';
            }
            line = newline ? (authum_char_t *)(newline + 1) : end;

            number++;
            parsed = monitoring_allowlist_parse_line(buffer, &entry);
            if (parsed < 0)
            {
                authum_logger_warning(&g_monitoring_logger,
                                      "%s:%d: not an IP or CIDR — skipped: %s", leaf, number,
                                      buffer);
                continue;
            }
            if (parsed == 0)
            {
                continue;
            }
            if (g_count == MONITORING_ALLOWLIST_MAX)
            {
                authum_logger_warning(&g_monitoring_logger,
                                      "%s: more than %d entries — rest ignored", leaf,
                                      MONITORING_ALLOWLIST_MAX);
                break;
            }
            g_entries[g_count++] = entry;
        }
    }
    return (int)g_count;
}

authum_bool_t
monitoring_allowlist_contains(authum_u32_t ip)
{
    return monitoring_allowlist_match(g_entries, g_count, ip);
}

authum_usize_t
monitoring_allowlist_count(void)
{
    return g_count;
}

const monitoring_allowlist_entry_t *
monitoring_allowlist_at(authum_usize_t index)
{
    return index < g_count ? &g_entries[index] : authum_null;
}
