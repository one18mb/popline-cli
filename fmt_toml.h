/* fmt_toml.h — TOML ↔ PopLine conversion declarations (stub) */
#ifndef FMT_TOML_H
#define FMT_TOML_H

#include "popline.h"

pln_value_t *fmt_toml_parse(const char *text);   /* TOML text → PopLine DOM */
char       *fmt_toml_dumps(pln_value_t *v);      /* PopLine DOM → TOML text */

#endif
