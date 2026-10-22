#ifndef MONITORING_BRIDGE_H
#define MONITORING_BRIDGE_H

#include <authum/client.h>

authum_bool_t
monitoring_authum_start(authum_host_t *host);
void
monitoring_authum_stop(void);

#endif /* MONITORING_BRIDGE_H */
