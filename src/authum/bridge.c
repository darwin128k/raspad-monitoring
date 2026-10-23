#include <monitoring/bridge.h>

#include <monitoring/command.h>
#include <monitoring/config.h>
#include <monitoring/hook.h>
#include <monitoring/host.h>
#include <monitoring/server.h>

#include <authum/host.h>
#include <authum/logger.h>
#include <authum/null.h>
#include <authum/path.h>

authum_host_t g_monitoring_host;
authum_logger_t g_monitoring_logger;

authum_bool_t
monitoring_authum_start(authum_host_t *host)
{
    if (authum_null_eq(host))
    {
        return authum_bool_false;
    }

    authum_host_copy(&g_monitoring_host, host);
    authum_logger_init(&g_monitoring_logger, &g_monitoring_host);

    if (authum_null_eq(authum_host_env(&g_monitoring_host)))
    {
        authum_logger_error(&g_monitoring_logger, "host facade incomplete (needs env)");
        return authum_bool_false;
    }

    monitoring_config_load();

    authum_logger_info(&g_monitoring_logger, "start — data dir: %s",
                       authum_path_as_cstr(authum_host_env_module_data(&g_monitoring_host)));

    monitoring_server_resolve();
    monitoring_command_load_list();

    if (!monitoring_hook_install())
    {
        authum_logger_error(&g_monitoring_logger, "hook install FAILED — queries left to Steam");
        return authum_bool_false;
    }

    /* Without the commands facade the module still works; only reload / list are gone. */
    if (authum_null_eq(authum_host_commands(&g_monitoring_host)) ||
        !monitoring_command_start(&g_monitoring_host))
    {
        authum_logger_warning(&g_monitoring_logger,
                              "console commands unavailable (authum monitoring reload / list)");
    }

    authum_logger_info(&g_monitoring_logger, "ready (info_answer=%s scope=%s)",
                       monitoring_info_answer_name(g_monitoring_config.info_answer),
                       monitoring_info_answer_scope_name(g_monitoring_config.info_answer_scope));
    return authum_bool_true;
}

void
monitoring_authum_stop(void)
{
    monitoring_hook_remove();
    authum_logger_info(&g_monitoring_logger, "stop");
    authum_logger_close(&g_monitoring_logger);
    authum_host_clear(&g_monitoring_host);
}
