#ifndef MONITORING_COMMAND_H
#define MONITORING_COMMAND_H

#include <authum/command.h>
#include <authum/host.h>

/* Console / RCON: `authum monitoring reload`, `authum monitoring list`. */
authum_bool_t
monitoring_command_start(const authum_host_t *host);

/* Load monitorings.ini and log the result (start and `reload`). */
int
monitoring_command_load_list(void);

authum_bool_t
monitoring_command_reload(const authum_command_t *cmd, authum_ptr user);

authum_bool_t
monitoring_command_list(const authum_command_t *cmd, authum_ptr user);

#endif /* MONITORING_COMMAND_H */
