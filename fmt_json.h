/* fmt_json.h — JSON → PopLine conversion (parse only, use sax_to_json for output) */
#ifndef FMT_JSON_H
#define FMT_JSON_H

#include "popline.h"

pln_value_t *fmt_json_parse(const char *json);   /* JSON text → PopLine DOM */

#endif
