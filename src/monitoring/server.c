#include <monitoring/server.h>

#include <monitoring/config.h>
#include <monitoring/host.h>

#include <authum/cstr.h>
#include <authum/file.h>
#include <authum/host.h>
#include <authum/logger.h>
#include <authum/memory.h>
#include <authum/net.h>
#include <authum/path.h>

/* Used only when neither the cfg nor the files say anything. */
static const authum_i32_t g_fallback_appid = 10;
static const authum_char_t g_fallback_version[] = "1.1.2.7/Stdio";

static authum_char_t g_detected_ip[AUTHUM_NET_IPV4_TEXT_SIZE];

authum_i32_t
monitoring_server_read_appid(const authum_path_t *path)
{
    authum_file_t file;

    authum_file_init(&file);
    if (!authum_file_open(&file, path, authum_file_mode_read))
    {
        return 0;
    }

    authum_char_t text[64];
    authum_usize_t size = 0;

    if (!authum_file_read_all(&file, text, sizeof text, &size) || size == 0 || size >= sizeof text)
    {
        authum_file_close(&file);
        return 0;
    }
    authum_file_close(&file);
    text[size] = '\0';

    /* Only the first line carries the appid. */
    {
        authum_char_t *newline = (authum_char_t *)authum_cstr_find_char(text, '\n');

        if (newline)
        {
            *newline = '\0';
        }
    }

    /* The number, then nothing but white space. */
    authum_char_t *start = text;

    while (authum_char_is_space(*start))
    {
        start++;
    }
    authum_char_t *stop = start + authum_cstr_len(start);

    while (stop > start && authum_char_is_space(stop[-1]))
    {
        stop--;
    }
    *stop = '\0';

    authum_i32_t value = 0;

    if (!authum_cstr_parse_i32(start, &value) || value < 1 || value > 0xffff)
    {
        return 0;
    }
    return value;
}

/* The length of @p value up to the first space, tab, CR or LF. */
static authum_usize_t
monitoring_server_value_len(const authum_char_t *value)
{
    static const authum_char_t stops[] = " \t\r\n";
    authum_usize_t len = authum_cstr_len(value);
    authum_usize_t i;

    for (i = 0; i < sizeof stops - 1; i++)
    {
        const authum_char_t *at = authum_cstr_find_char(value, stops[i]);

        if (at && (authum_usize_t)(at - value) < len)
        {
            len = (authum_usize_t)(at - value);
        }
    }
    return len;
}

authum_bool_t
monitoring_server_read_version(const authum_path_t *path, authum_char_t *out,
                               authum_usize_t out_size)
{
    static const authum_char_t key[] = "PatchVersion=";
    authum_file_t file;

    authum_file_init(&file);
    if (!authum_file_open(&file, path, authum_file_mode_read))
    {
        return authum_bool_false;
    }

    static authum_char_t text[8192];
    authum_usize_t size = 0;

    if (!authum_file_read_all(&file, text, sizeof text, &size) || size >= sizeof text)
    {
        authum_file_close(&file);
        authum_logger_warning(&g_monitoring_logger, "steam.inf: over %d bytes — not read",
                              (int)sizeof text);
        return authum_bool_false;
    }
    authum_file_close(&file);
    text[size] = '\0';

    /* steam.inf is a flat list of "Key=Value" lines; the file API reads
     * bytes, so the lines are split here. */
    authum_char_t *line = text;
    authum_char_t *end = text + size;
    authum_bool_t found = authum_bool_false;

    while (line < end && !found)
    {
        authum_char_t *newline = (authum_char_t *)authum_cstr_find_char(line, '\n');
        authum_char_t *value;
        authum_usize_t len;

        if (newline)
        {
            *newline = '\0';
        }
        if (!authum_cstr_is_equal_n(line, key, sizeof key - 1, authum_bool_true))
        {
            line = newline ? newline + 1 : end;
            continue;
        }
        value = line + sizeof key - 1;
        len = monitoring_server_value_len(value);
        if (len == 0)
        {
            line = newline ? newline + 1 : end;
            continue;
        }
        value[len] = '\0';
        authum_cstr_format(out, out_size, "%s/Stdio", value);
        found = (len + sizeof "/Stdio" <= out_size) ? authum_bool_true : authum_bool_false;
        line = newline ? newline + 1 : end;
    }
    return found;
}

const authum_char_t *
monitoring_server_detected_ip(void)
{
    return g_detected_ip;
}

/* Log every address on the machine; keep the external one only when it is unique. */
static void
monitoring_server_detect_ip(void)
{
    authum_u32_t ips[8];

    const authum_usize_t internal = authum_net_ipv4_collect_internal(ips, 8u);

    for (authum_usize_t i = 0u; i < internal && i < 8u; i++)
    {
        authum_char_t text[AUTHUM_NET_IPV4_TEXT_SIZE];

        authum_net_ipv4_format(ips[i], text, sizeof text);
        authum_logger_info(&g_monitoring_logger, "internal IPv4 %s", text);
    }

    const authum_usize_t external = authum_net_ipv4_collect_external(ips, 8u);

    for (authum_usize_t i = 0u; i < external && i < 8u; i++)
    {
        authum_char_t text[AUTHUM_NET_IPV4_TEXT_SIZE];

        authum_net_ipv4_format(ips[i], text, sizeof text);
        authum_logger_info(&g_monitoring_logger, "external IPv4 %s", text);
    }

    g_detected_ip[0] = '\0';
    if (external == 1u)
    {
        authum_net_ipv4_format(ips[0], g_detected_ip, sizeof g_detected_ip);
    }
    else if (external > 1u)
    {
        authum_logger_warning(&g_monitoring_logger,
                              "%u external IPv4 on this machine — not guessing, set public_ip",
                              (unsigned)external);
    }
}

void
monitoring_server_resolve(void)
{
    const authum_path_t *game = authum_host_get_game_path(&g_monitoring_host);

    if (g_monitoring_config.appid > 0)
    {
        authum_logger_info(&g_monitoring_logger, "appid %d (monitoring.cfg)",
                           (int)g_monitoring_config.appid);
    }
    else
    {
        authum_path_t root;
        authum_path_t file;

        if (game && authum_path_assign_parent(&root, game) &&
            authum_path_join(&file, &root, "steam_appid.txt") &&
            (g_monitoring_config.appid = monitoring_server_read_appid(&file)) > 0)
        {
            authum_logger_info(&g_monitoring_logger, "appid %d (%s)",
                               (int)g_monitoring_config.appid, authum_path_get_cstr(&file));
        }
        else
        {
            g_monitoring_config.appid = g_fallback_appid;
            authum_logger_warning(
                &g_monitoring_logger,
                "appid: steam_appid.txt not found — using %d (set appid to override)",
                (int)g_fallback_appid);
        }
    }

    if (g_monitoring_config.version[0])
    {
        authum_logger_info(&g_monitoring_logger, "version %s (monitoring.cfg)",
                           g_monitoring_config.version);
    }
    else
    {
        authum_path_t file;

        if (game && authum_path_join(&file, game, "steam.inf") &&
            monitoring_server_read_version(&file, g_monitoring_config.version,
                                           sizeof g_monitoring_config.version))
        {
            authum_logger_info(&g_monitoring_logger, "version %s (%s)", g_monitoring_config.version,
                               authum_path_get_cstr(&file));
        }
        else
        {
            authum_cstr_copy(g_monitoring_config.version, sizeof g_monitoring_config.version,
                             g_fallback_version);
            authum_logger_warning(
                &g_monitoring_logger,
                "version: no PatchVersion in steam.inf — using %s (set version to "
                "override)",
                g_fallback_version);
        }
    }

    monitoring_server_detect_ip();
}
