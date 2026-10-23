#include <monitoring/server.h>

#include <monitoring/config.h>
#include <monitoring/host.h>

#include <authum/host.h>
#include <authum/logger.h>
#include <authum/net.h>
#include <authum/path.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Used only when neither the cfg nor the files say anything. */
static const authum_i32_t g_fallback_appid = 10;
static const char g_fallback_version[] = "1.1.2.7/Stdio";

static char g_detected_ip[AUTHUM_NET_IPV4_TEXT_SIZE];

authum_i32_t
monitoring_server_read_appid(const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[32];
    char *end;
    long value;

    if (!fp)
    {
        return 0;
    }
    if (!fgets(line, (int)sizeof line, fp))
    {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    value = strtol(line, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
    {
        end++;
    }
    if (end == line || *end != '\0' || value < 1 || value > 0xffff)
    {
        return 0;
    }
    return (authum_i32_t)value;
}

authum_bool_t
monitoring_server_read_version(const char *path, char *out, size_t out_size)
{
    static const char key[] = "PatchVersion=";
    FILE *fp = fopen(path, "r");
    char line[128];
    authum_bool_t found = authum_bool_false;

    if (!fp)
    {
        return authum_bool_false;
    }
    while (!found && fgets(line, (int)sizeof line, fp))
    {
        char *value;
        size_t len;

        if (strncasecmp(line, key, sizeof key - 1) != 0)
        {
            continue;
        }
        value = line + sizeof key - 1;
        len = strcspn(value, " \t\r\n");
        if (len == 0)
        {
            continue;
        }
        value[len] = '\0';
        found = snprintf(out, out_size, "%s/Stdio", value) < (int)out_size;
    }
    fclose(fp);
    return found;
}

const char *
monitoring_server_detected_ip(void)
{
    return g_detected_ip;
}

/* Log every address on the machine; keep the external one only when it is unique. */
static void
monitoring_server_detect_ip(void)
{
    authum_u32_t ips[8];
    authum_char_t text[AUTHUM_NET_IPV4_TEXT_SIZE];
    authum_usize_t internal = authum_net_ipv4_internal(ips, 8u);
    authum_usize_t external;
    authum_usize_t i;

    for (i = 0u; i < internal && i < 8u; i++)
    {
        authum_net_ipv4_format(ips[i], text, sizeof text);
        authum_logger_info(&g_monitoring_logger, "internal IPv4 %s", text);
    }
    external = authum_net_ipv4_external(ips, 8u);
    for (i = 0u; i < external && i < 8u; i++)
    {
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
    const authum_path_t *game = authum_host_env_game(&g_monitoring_host);
    authum_path_t root;
    authum_path_t file;

    if (g_monitoring_config.appid > 0)
    {
        authum_logger_info(&g_monitoring_logger, "appid %d (monitoring.cfg)",
                           (int)g_monitoring_config.appid);
    }
    else if (game && authum_path_parent(&root, game) &&
             authum_path_join(&file, &root, "steam_appid.txt") &&
             (g_monitoring_config.appid =
                  monitoring_server_read_appid(authum_path_as_cstr(&file))) > 0)
    {
        authum_logger_info(&g_monitoring_logger, "appid %d (%s)", (int)g_monitoring_config.appid,
                           authum_path_as_cstr(&file));
    }
    else
    {
        g_monitoring_config.appid = g_fallback_appid;
        authum_logger_warning(&g_monitoring_logger,
                              "appid: steam_appid.txt not found — using %d (set appid to override)",
                              (int)g_fallback_appid);
    }

    if (g_monitoring_config.version[0])
    {
        authum_logger_info(&g_monitoring_logger, "version %s (monitoring.cfg)",
                           g_monitoring_config.version);
    }
    else if (game && authum_path_join(&file, game, "steam.inf") &&
             monitoring_server_read_version(authum_path_as_cstr(&file), g_monitoring_config.version,
                                            sizeof g_monitoring_config.version))
    {
        authum_logger_info(&g_monitoring_logger, "version %s (%s)", g_monitoring_config.version,
                           authum_path_as_cstr(&file));
    }
    else
    {
        snprintf(g_monitoring_config.version, sizeof g_monitoring_config.version, "%s",
                 g_fallback_version);
        authum_logger_warning(&g_monitoring_logger,
                              "version: no PatchVersion in steam.inf — using %s (set version to "
                              "override)",
                              g_fallback_version);
    }

    monitoring_server_detect_ip();
}
