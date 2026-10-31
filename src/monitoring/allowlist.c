#include <monitoring/allowlist.h>

#include <monitoring/host.h>

#include <authum/file.h>
#include <authum/host.h>
#include <authum/logger.h>
#include <authum/path.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static monitoring_allowlist_entry_t g_entries[MONITORING_ALLOWLIST_MAX];
static size_t g_count;

static int
monitoring_allowlist_parse_octet(const char **p, uint32_t *out)
{
    unsigned value = 0;
    int digits = 0;

    while (isdigit((unsigned char)**p))
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
monitoring_allowlist_parse_line(const char *line, monitoring_allowlist_entry_t *out)
{
    const char *p = line;
    uint32_t ip = 0;
    uint32_t octet;
    unsigned bits = 32;
    int i;

    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';')
    {
        return 0;
    }

    for (i = 0; i < 4; i++)
    {
        if (!monitoring_allowlist_parse_octet(&p, &octet) || (i < 3 && *p++ != '.'))
        {
            return -1;
        }
        ip = (ip << 8) | octet;
    }
    if (*p == '/')
    {
        uint32_t value;

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
    out->bits = (uint8_t)bits;
    return 1;
}

authum_bool_t
monitoring_allowlist_match(const monitoring_allowlist_entry_t *entries, size_t count, uint32_t ip)
{
    size_t i;

    for (i = 0; i < count; i++)
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
    authum_path_t path;
    authum_file_t file;
    static authum_char_t text[MONITORING_ALLOWLIST_TEXT_MAX];
    authum_usize_t size = 0;
    authum_char_t *line;
    authum_char_t *end;
    int number = 0;
    monitoring_allowlist_entry_t entry;

    g_count = 0;
    /* modules/monitoring/configs/monitorings.ini */
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

    /* The text is split here, not by the file API: it reads bytes, not lines. */
    line = text;
    end = text + size;
    while (line < end)
    {
        char buffer[256];
        authum_char_t *newline;
        size_t length;
        int parsed;

        newline = (authum_char_t *)memchr(line, '\n', (size_t)(end - line));
        length = newline ? (size_t)(newline - line) : (size_t)(end - line);
        if (length >= sizeof buffer)
        {
            length = sizeof buffer - 1;
        }
        memcpy(buffer, line, length);
        buffer[length] = '\0';
        if (length > 0 && buffer[length - 1] == '\r')
        {
            buffer[length - 1] = '\0';
        }
        line = newline ? newline + 1 : end;

        number++;
        parsed = monitoring_allowlist_parse_line(buffer, &entry);
        if (parsed < 0)
        {
            authum_logger_warning(&g_monitoring_logger, "%s:%d: not an IP or CIDR — skipped: %s",
                                  leaf, number, buffer);
            continue;
        }
        if (parsed == 0)
        {
            continue;
        }
        if (g_count == MONITORING_ALLOWLIST_MAX)
        {
            authum_logger_warning(&g_monitoring_logger, "%s: more than %d entries — rest ignored",
                                  leaf, MONITORING_ALLOWLIST_MAX);
            break;
        }
        g_entries[g_count++] = entry;
    }
    return (int)g_count;
}

authum_bool_t
monitoring_allowlist_contains(uint32_t ip)
{
    return monitoring_allowlist_match(g_entries, g_count, ip);
}

size_t
monitoring_allowlist_count(void)
{
    return g_count;
}

const monitoring_allowlist_entry_t *
monitoring_allowlist_at(size_t index)
{
    return index < g_count ? &g_entries[index] : NULL;
}
