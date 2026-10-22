#ifndef MONITORING_ALLOWLIST_H
#define MONITORING_ALLOWLIST_H

#include <authum/bool.h>
#include <authum/char.h>

#include <stddef.h>
#include <stdint.h>

/*
 * Known monitoring pollers (monitorings.ini) for info_answer_scope = list:
 * only these get answers that skip the Steam challenge. One address or CIDR
 * per line ("1.2.3.4", "1.2.3.0/24"); text after it, and lines starting with
 * '#' or ';', are comments. Loaded at start and on `authum monitoring reload`.
 */
#define MONITORING_ALLOWLIST_MAX 256

typedef struct monitoring_allowlist_entry
{
    uint32_t net;  /* host byte order, already masked */
    uint32_t mask; /* host byte order */
    uint8_t bits;
} monitoring_allowlist_entry_t;

/* Parse one line. 1 = entry, 0 = blank / comment, -1 = malformed. */
int
monitoring_allowlist_parse_line(const char *line, monitoring_allowlist_entry_t *out);

authum_bool_t
monitoring_allowlist_match(const monitoring_allowlist_entry_t *entries, size_t count, uint32_t ip);

/* (Re)load @p leaf from the module dir into the live table. Returns entries
 * loaded, or -1 when the file cannot be opened (the table is then emptied). */
int
monitoring_allowlist_load(const authum_char_t *leaf);

authum_bool_t
monitoring_allowlist_contains(uint32_t ip);

size_t
monitoring_allowlist_count(void);

const monitoring_allowlist_entry_t *
monitoring_allowlist_at(size_t index);

#endif /* MONITORING_ALLOWLIST_H */
