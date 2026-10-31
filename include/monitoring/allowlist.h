#ifndef MONITORING_ALLOWLIST_H
#define MONITORING_ALLOWLIST_H

#include <authum/bool.h>
#include <authum/byte.h>
#include <authum/char.h>
#include <authum/size.h>

/*
 * Known monitoring pollers (monitorings.ini) for info_answer_scope = list:
 * only these get answers that skip the Steam challenge. One address or CIDR
 * per line ("1.2.3.4", "1.2.3.0/24"); text after it, and lines starting with
 * '#' or ';', are comments. Loaded at start and on `authum monitoring reload`.
 */
#define MONITORING_ALLOWLIST_MAX 256

/* How much text the allow list file may hold; it is read whole and split
 * into lines here (the file API reads bytes, not lines). */
#define MONITORING_ALLOWLIST_TEXT_MAX 16384

typedef struct monitoring_allowlist_entry
{
    authum_u32_t net;  /* host byte order, already masked */
    authum_u32_t mask; /* host byte order */
    authum_u8_t bits;
} monitoring_allowlist_entry_t;

/* Parse one line. 1 = entry, 0 = blank / comment, -1 = malformed. */
int
monitoring_allowlist_parse_line(const authum_char_t *line, monitoring_allowlist_entry_t *out);

/* Whether @p ip falls into one of the @p count entries. */
authum_bool_t
monitoring_allowlist_match(const monitoring_allowlist_entry_t *entries, authum_usize_t count,
                           authum_u32_t ip);

/* (Re)load @p leaf from the module dir into the live table. Returns entries
 * loaded, or -1 when the file cannot be opened (the table is then emptied). */
int
monitoring_allowlist_load(const authum_char_t *leaf);

/* Whether @p ip is in the live table. */
authum_bool_t
monitoring_allowlist_contains(authum_u32_t ip);

/* How many entries the live table holds. */
authum_usize_t
monitoring_allowlist_count(void);

/* The entry at @p index, or null when there is none. */
const monitoring_allowlist_entry_t *
monitoring_allowlist_at(authum_usize_t index);

#endif /* MONITORING_ALLOWLIST_H */
