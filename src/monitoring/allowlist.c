#include <monitoring/allowlist.h>

#include <monitoring/host.h>

#include <authum/host.h>
#include <authum/logger.h>
#include <authum/path.h>

#include <ctype.h>
#include <stdio.h>
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
    FILE *fp;
    char line[256];
    int number = 0;
    monitoring_allowlist_entry_t entry;

    g_count = 0;
    /* addons/authum/configs/monitoring/monitorings.ini */
    if (!authum_host_module_config_path(&g_monitoring_host, leaf, &path))
    {
        return -1;
    }
    fp = fopen(authum_path_as_cstr(&path), "r");
    if (!fp)
    {
        return -1;
    }
    while (fgets(line, (int)sizeof line, fp))
    {
        int parsed;

        number++;
        parsed = monitoring_allowlist_parse_line(line, &entry);
        if (parsed < 0)
        {
            line[strcspn(line, "\r\n")] = '\0';
            authum_logger_warning(&g_monitoring_logger, "%s:%d: not an IP or CIDR — skipped: %s",
                                  leaf, number, line);
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
    fclose(fp);
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
