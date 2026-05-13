/* popline_json.h — JSON ↔ PopLine conversion declarations */
#ifndef POPLINE_JSON_H
#define POPLINE_JSON_H

#include "popline.h"

pln_value_t *pln_loads_json(const char *json);   /* JSON text → PopLine DOM */
char       *pln_dumps_json(pln_value_t *v);      /* PopLine DOM → JSON text */

#endif
