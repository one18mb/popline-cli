/* fmt_ini.h — INI ↔ PopLine conversion declarations (stub) */
#ifndef FMT_INI_H
#define FMT_INI_H

#include "popline.h"

pln_value_t *fmt_ini_parse(const char *text);    /* INI text → PopLine DOM */
char       *fmt_ini_dumps(pln_value_t *v);       /* PopLine DOM → INI text */

#endif
