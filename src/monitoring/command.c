#include <monitoring/command.h>

#include <monitoring/allowlist.h>
#include <monitoring/config.h>
#include <monitoring/host.h>

#include <authum/command/def.h>
#include <authum/logger.h>
#include <authum/null.h>

static const authum_command_def_t g_monitoring_commands[] = {
    {"reload", authum_null, "re-read monitorings.ini (known monitoring pollers)",
     monitoring_command_reload, authum_null},
    {"list", authum_null, "show info_answer_scope and the loaded monitorings",
     monitoring_command_list, authum_null},
};

authum_bool_t
monitoring_command_start(const authum_host_t *host)
{
    return authum_host_command_add_all(host, g_monitoring_commands,
                                       sizeof g_monitoring_commands /
                                           sizeof g_monitoring_commands[0]);
}

int
monitoring_command_load_list(void)
{
    int loaded = monitoring_allowlist_load(g_monitoring_config.monitorings);

    if (loaded < 0)
    {
        if (g_monitoring_config.info_answer_scope == monitoring_info_answer_scope_list)
        {
            authum_logger_warning(&g_monitoring_logger,
                                  "%s missing — info_answer_scope=list answers nobody directly",
                                  g_monitoring_config.monitorings);
        }
        return loaded;
    }
    authum_logger_info(&g_monitoring_logger, "%s: %d monitoring poller(s)",
                       g_monitoring_config.monitorings, loaded);
    return loaded;
}

authum_bool_t
monitoring_command_reload(const authum_command_t *cmd, authum_ptr user)
{
    int loaded;

    (void)user;
    loaded = monitoring_command_load_list();
    if (loaded < 0)
    {
        authum_command_printf(cmd, "monitoring: cannot open %s", g_monitoring_config.monitorings);
    }
    else
    {
        authum_command_printf(cmd, "monitoring: %s reloaded, %d entr%s",
                              g_monitoring_config.monitorings, loaded, loaded == 1 ? "y" : "ies");
    }
    return authum_bool_true;
}

authum_bool_t
monitoring_command_list(const authum_command_t *cmd, authum_ptr user)
{
    const monitoring_allowlist_entry_t *entry;
    size_t i;

    (void)user;
    authum_command_printf(cmd, "monitoring: info_answer=%s scope=%s list=%s (%u entries)",
                          monitoring_info_answer_name(g_monitoring_config.info_answer),
                          monitoring_info_answer_scope_name(g_monitoring_config.info_answer_scope),
                          g_monitoring_config.monitorings, (unsigned)monitoring_allowlist_count());
    for (i = 0; (entry = monitoring_allowlist_at(i)) != authum_null; i++)
    {
        authum_command_printf(cmd, "  %u.%u.%u.%u/%u", (unsigned)(entry->net >> 24),
                              (unsigned)((entry->net >> 16) & 0xffu),
                              (unsigned)((entry->net >> 8) & 0xffu), (unsigned)(entry->net & 0xffu),
                              (unsigned)entry->bits);
    }
    return authum_bool_true;
}
