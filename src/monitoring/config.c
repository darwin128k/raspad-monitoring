#include <monitoring/config.h>
#include <monitoring/host.h>

#include <authum/cfg.h>
#include <authum/cstr.h>
#include <authum/host.h>
#include <authum/null.h>
#include <authum/path.h>

monitoring_config_t g_monitoring_config = {
    .info_answer = monitoring_info_answer_new,
    .info_answer_scope = monitoring_info_answer_scope_all,
    .monitorings = "monitorings.ini",
    .legacy_info = authum_bool_true,
    .legacy_details = authum_bool_true,
    .legacy_players = authum_bool_true,
    .legacy_rules = authum_bool_false,
    .public_ip = "",
    .appid = 0,    /* 0 = steam_appid.txt (server.c) */
    .version = "", /* "" = steam.inf PatchVersion + "/Stdio" */
    .ratelimit_per_ip = 5,
    .debug = authum_bool_false,

};

static const authum_cfg_item_t g_monitoring_config_items[] = {
    {"monitorings", authum_cfg_kind_cstr, g_monitoring_config.monitorings,
     sizeof g_monitoring_config.monitorings},
    {"legacy_info", authum_cfg_kind_bool, &g_monitoring_config.legacy_info, 0},
    {"legacy_details", authum_cfg_kind_bool, &g_monitoring_config.legacy_details, 0},
    {"legacy_players", authum_cfg_kind_bool, &g_monitoring_config.legacy_players, 0},
    {"legacy_rules", authum_cfg_kind_bool, &g_monitoring_config.legacy_rules, 0},
    {"public_ip", authum_cfg_kind_cstr, g_monitoring_config.public_ip,
     sizeof g_monitoring_config.public_ip},
    {"appid", authum_cfg_kind_i32, &g_monitoring_config.appid, 0},
    {"version", authum_cfg_kind_cstr, g_monitoring_config.version,
     sizeof g_monitoring_config.version},
    {"ratelimit_per_ip", authum_cfg_kind_i32, &g_monitoring_config.ratelimit_per_ip, 0},
    {"debug", authum_cfg_kind_bool, &g_monitoring_config.debug, 0},
};

static const authum_char_t *const g_monitoring_info_answer_names[] = {
    "steam",
    "new",
    "old",
    "hybrid",
};

const authum_char_t *
monitoring_info_answer_name(monitoring_info_answer_t answer)
{
    if ((unsigned)answer >=
        sizeof g_monitoring_info_answer_names / sizeof g_monitoring_info_answer_names[0])
    {
        return "?";
    }
    return g_monitoring_info_answer_names[answer];
}

static const authum_char_t *const g_monitoring_info_answer_scope_names[] = {
    "all",
    "list",
};

const authum_char_t *
monitoring_info_answer_scope_name(monitoring_info_answer_scope_t scope)
{
    if ((unsigned)scope >= sizeof g_monitoring_info_answer_scope_names /
                               sizeof g_monitoring_info_answer_scope_names[0])
    {
        return "?";
    }
    return g_monitoring_info_answer_scope_names[scope];
}

static void
monitoring_config_parse_info_answer_scope(const authum_char_t *value)
{
    if (authum_cstr_eq(value, "all", authum_bool_true))
    {
        g_monitoring_config.info_answer_scope = monitoring_info_answer_scope_all;
    }
    else if (authum_cstr_eq(value, "list", authum_bool_true))
    {
        g_monitoring_config.info_answer_scope = monitoring_info_answer_scope_list;
    }
    else
    {
        authum_logger_warning(
            &g_monitoring_logger, "info_answer_scope '%s' unknown — keeping %s", value,
            monitoring_info_answer_scope_name(g_monitoring_config.info_answer_scope));
    }
}

static void
monitoring_config_parse_info_answer(const authum_char_t *value)
{
    unsigned i;

    for (i = 0;
         i < sizeof g_monitoring_info_answer_names / sizeof g_monitoring_info_answer_names[0]; i++)
    {
        if (authum_cstr_eq(value, g_monitoring_info_answer_names[i], authum_bool_true))
        {
            g_monitoring_config.info_answer = (monitoring_info_answer_t)i;
            return;
        }
    }
    authum_logger_warning(&g_monitoring_logger, "info_answer '%s' unknown — keeping %s", value,
                          monitoring_info_answer_name(g_monitoring_config.info_answer));
}

static void
monitoring_config_apply_file(const authum_char_t *leaf)
{
    authum_path_t path;
    authum_cfg_t cfg;
    authum_char_t answer[32];

    /* addons/authum/configs/<leaf> */
    if (!authum_host_config_path(&g_monitoring_host, leaf, &path) ||
        !authum_cfg_open(&cfg, &path))
    {
        return;
    }

    authum_cfg_apply_items(
        &cfg, g_monitoring_config_items,
        (authum_usize_t)(sizeof g_monitoring_config_items / sizeof g_monitoring_config_items[0]));

    if (authum_cfg_apply_cstr(&cfg, "info_answer", answer, sizeof answer))
    {
        monitoring_config_parse_info_answer(answer);
    }

    if (authum_cfg_apply_cstr(&cfg, "info_answer_scope", answer, sizeof answer))
    {
        monitoring_config_parse_info_answer_scope(answer);
    }

    authum_cfg_close(&cfg);
    authum_logger_info(&g_monitoring_logger, "applied %s", leaf);
}

void
monitoring_config_load(void)
{
    monitoring_config_apply_file("monitoring.cfg");
    monitoring_config_apply_file("monitoring.local.cfg");
    if (g_monitoring_config.appid < 0 || g_monitoring_config.appid > 0xffff)
    {
        g_monitoring_config.appid = 0;
    }
    if (g_monitoring_config.ratelimit_per_ip < 0)
    {
        g_monitoring_config.ratelimit_per_ip = 0;
    }
    authum_logger_info(
        &g_monitoring_logger,
        "cfg info_answer=%s scope=%s (%s) legacy info=%d details=%d players=%d rules=%d "
        "public_ip=%s appid=%d version=%s ratelimit_per_ip=%d debug=%d",
        monitoring_info_answer_name(g_monitoring_config.info_answer),
        monitoring_info_answer_scope_name(g_monitoring_config.info_answer_scope),
        g_monitoring_config.monitorings, (int)g_monitoring_config.legacy_info,
        (int)g_monitoring_config.legacy_details, (int)g_monitoring_config.legacy_players,
        (int)g_monitoring_config.legacy_rules,
        g_monitoring_config.public_ip[0] ? g_monitoring_config.public_ip : "(ip cvar)",
        (int)g_monitoring_config.appid, g_monitoring_config.version,
        (int)g_monitoring_config.ratelimit_per_ip, (int)g_monitoring_config.debug);
}
