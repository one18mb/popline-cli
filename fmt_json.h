/* fmt_json.h — JSON ↔ PopLine conversion declarations */
#ifndef FMT_JSON_H
#define FMT_JSON_H

#include "popline.h"

pln_value_t *fmt_json_parse(const char *json);   /* JSON text → PopLine DOM */
char       *fmt_json_dumps(pln_value_t *v);      /* PopLine DOM → JSON text */

#endif
