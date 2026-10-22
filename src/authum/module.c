#include <monitoring/bridge.h>

#include <authum/client.h>

AUTHUM_MODULE_EXPORT const authum_module_api_t authum_module_api = {
    .start = monitoring_authum_start,
    .stop = monitoring_authum_stop,
};
