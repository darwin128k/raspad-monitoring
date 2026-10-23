#include <authum/client.h>

AUTHUM_MODULE_EXPORT authum_module_info_t authum_module_info = {
    .abi = AUTHUM_MODULE_ABI,
    .name = "monitoring",
    /* From the VERSION file (CMakeLists.txt). */
    .version = AUTHUM_VERSION(MONITORING_VERSION_MAJOR, MONITORING_VERSION_MINOR,
                              MONITORING_VERSION_PATCH),
    .date = "2026-10-22",
    .author = "raspad",
    .url = "",
    .logtag = "MONITORING",
};
