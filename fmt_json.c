/* popline_json.c — JSON → PopLine DOM conversion (parse only) */
#include "popline.h"
#include <cjson/cJSON.h>

static pln_value_t *cjson_to_pl(cJSON *c) {
    if (!c) return NULL;
    if (cJSON_IsNull(c))   return pln_value_new_null();
    if (cJSON_IsFalse(c))  return pln_value_new_bool(0);
    if (cJSON_IsTrue(c))   return pln_value_new_bool(1);
    if (cJSON_IsNumber(c)) {
        double d = c->valuedouble;
        long long ll = (long long)d;
        if ((double)ll == d)
            return pln_value_new_int(ll);
        return pln_value_new_float(d);
    }
    if (cJSON_IsString(c)) return pln_value_new_string(c->valuestring);
    if (cJSON_IsObject(c)) {
        pln_value_t *v = pln_value_new_object();
        cJSON *item;
        cJSON_ArrayForEach(item, c) {
            if (item->string)
                pln_value_add_to_object(v, item->string, cjson_to_pl(item));
        }
        return v;
    }
    if (cJSON_IsArray(c)) {
        pln_value_t *v = pln_value_new_array();
        cJSON *item;
        cJSON_ArrayForEach(item, c)
            pln_value_add_to_array(v, cjson_to_pl(item));
        return v;
    }
    return pln_value_new_null();
}

pln_value_t *fmt_json_parse(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;
    pln_value_t *v = cjson_to_pl(root);
    cJSON_Delete(root);
    return v;
}

