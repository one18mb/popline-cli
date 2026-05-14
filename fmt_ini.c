/* fmt_ini.c — INI → PopLine DOM parser */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popline.h"

/* ─── Parser: INI text → PopLine DOM ───────────────────── */

pln_value_t *fmt_ini_parse(const char *text) {
    pln_value_t *root = pln_value_new_object();
    pln_value_t *cur_section = NULL;

    char *copy = strdup(text);
    if (!copy) { pln_value_free(root); return NULL; }

    char *line = copy;
    while (line && *line) {
        /* Isolate one line */
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';
        else next = line + strlen(line);

        /* Skip leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Skip empty lines and comment lines */
        if (*p == '\0' || *p == ';' || *p == '#') {
            line = next;
            continue;
        }

        /* ── Section header [section] ──────────────────── */
        if (*p == '[') {
            p++;
            char *end = strchr(p, ']');
            if (!end) { free(copy); pln_value_free(root); return NULL; }
            *end = '\0';

            /* Trim leading whitespace from section name */
            char *sn = p;
            while (*sn == ' ' || *sn == '\t') sn++;

            /* Trim trailing whitespace */
            while (end > sn && (*(end - 1) == ' ' || *(end - 1) == '\t')) end--;
            *end = '\0';

            if (*sn == '\0') { line = next; continue; }  /* skip empty name */

            cur_section = pln_value_new_object();
            pln_value_add_to_object(root, sn, cur_section);

            line = next;
            continue;
        }

        /* ── Key = value ───────────────────────────────── */
        char *eq = strchr(p, '=');
        if (!eq) { free(copy); pln_value_free(root); return NULL; }
        *eq = '\0';

        /* Trim trailing whitespace from key */
        {
            char *ke = eq;
            while (ke > p && (*(ke - 1) == ' ' || *(ke - 1) == '\t')) ke--;
            *ke = '\0';
        }
        /* Trim leading whitespace from key (belt-and-suspenders) */
        char *key = p;
        while (*key == ' ' || *key == '\t') key++;
        if (*key == '\0') { free(copy); pln_value_free(root); return NULL; }

        /* Value — skip leading whitespace */
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;

        /* Detect inline comment (; or # preceded by whitespace or at value start) */
        {
            char *c = val;
            while (*c) {
                if ((*c == ';' || *c == '#') &&
                    (c == val || *(c - 1) == ' ' || *(c - 1) == '\t')) {
                    *c = '\0';
                    break;
                }
                c++;
            }
        }

        /* Trim trailing whitespace from value */
        {
            int vl = (int)strlen(val);
            while (vl > 0 && (val[vl - 1] == ' ' || val[vl - 1] == '\t')) vl--;
            val[vl] = '\0';
        }

        pln_value_t *val_obj;
        /* Type detection for INI values */
        if (strcmp(val, "true") == 0 || strcmp(val, "yes") == 0 || strcmp(val, "on") == 0)
            val_obj = pln_value_new_bool(1);
        else if (strcmp(val, "false") == 0 || strcmp(val, "no") == 0 || strcmp(val, "off") == 0)
            val_obj = pln_value_new_bool(0);
        else if (strcmp(val, "null") == 0 || strcmp(val, "none") == 0)
            val_obj = pln_value_new_null();
        else if ((val[0] == '-' || (val[0] >= '0' && val[0] <= '9'))) {
            char *end;
            if (strchr(val, '.') || strchr(val, 'e') || strchr(val, 'E')) {
                double d = strtod(val, &end);
                if (*end == '\0') val_obj = pln_value_new_float(d);
                else val_obj = pln_value_new_string(val);
            } else {
                long long ll = strtoll(val, &end, 10);
                if (*end == '\0') val_obj = pln_value_new_int(ll);
                else val_obj = pln_value_new_string(val);
            }
        } else {
            val_obj = pln_value_new_string(val);
        }
        if (cur_section)
            pln_value_add_to_object(cur_section, key, val_obj);
        else
            pln_value_add_to_object(root, key, val_obj);

        line = next;
    }

    free(copy);
    return root;
}
