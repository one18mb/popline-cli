/* fmt_ini.c — INI ↔ PopLine DOM converter */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popline.h"

/* ─── Dynamic string builder (for serialization output) ── */

typedef struct {
    char *buf;
    int   len;
    int   cap;
} sb_t;

static void sb_init(sb_t *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_grow(sb_t *sb, int needed) {
    if (needed <= sb->cap) return;
    int new_cap = sb->cap ? sb->cap : 128;
    while (new_cap < needed) new_cap *= 2;
    char *nb = (char *)realloc(sb->buf, new_cap);
    if (!nb) { fprintf(stderr, "OOM\n"); abort(); }
    sb->buf = nb;
    sb->cap = new_cap;
}

static void sb_append(sb_t *sb, const char *s) {
    if (!s) return;
    int slen = (int)strlen(s);
    sb_grow(sb, sb->len + slen + 1);
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

static void sb_append_char(sb_t *sb, char c) {
    sb_grow(sb, sb->len + 2);
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

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

/* ─── Serializer: PopLine DOM → INI text ───────────────── */
/* Check if a value tree is representable in INI format */
static int ini_can_serialize(pln_value_t *v, int depth) {
    if (!v) return 1;
    switch (v->type) {
    case PLN_STRING: case PLN_INT: case PLN_FLOAT: case PLN_BOOL: case PLN_NULL:
        return 1;
    case PLN_ARRAY:
        return 0; /* INI has no array representation */
    case PLN_OBJECT:
        if (depth == 0) {
            for (pln_value_t *c = v->child; c; c = c->next) {
                if (!ini_can_serialize(c, c->type == PLN_OBJECT ? 1 : 0)) return 0;
            }
            return 1;
        }
        /* Section body or deeper: only scalar children allowed */
        for (pln_value_t *c = v->child; c; c = c->next) {
            if (c->type == PLN_OBJECT || c->type == PLN_ARRAY) return 0;
        }
        return 1;
    }
    return 1;
}

static void write_value(sb_t *sb, pln_value_t *v) {
    if (!v) { sb_append(sb, ""); return; }
    switch (v->type) {
    case PLN_STRING:
        sb_append(sb, v->data.string_val ? v->data.string_val : "");
        break;
    case PLN_INT: {
        char buf[32];
        snprintf(buf, sizeof buf, "%lld", v->data.int_val);
        sb_append(sb, buf);
        break;
    }
    case PLN_FLOAT: {
        char buf[64];
        snprintf(buf, sizeof buf, "%g", v->data.float_val);
        sb_append(sb, buf);
        break;
    }
    case PLN_BOOL:
        sb_append(sb, v->data.bool_val ? "true" : "false");
        break;
    case PLN_NULL:
        sb_append(sb, "null");
        break;
    default:
        /* Nested arrays / objects — not representable in flat INI; skip */
        break;
    }
}

char *fmt_ini_dumps(pln_value_t *v) {
    if (!v || v->type != PLN_OBJECT) return NULL;
    if (!ini_can_serialize(v, 0)) return NULL;

    sb_t sb;
    sb_init(&sb);

    pln_value_t *child = v->child;
    while (child) {
        if (!child->key) { child = child->next; continue; }

        if (child->type == PLN_OBJECT) {
            /* Section header */
            sb_append(&sb, "[");
            sb_append(&sb, child->key);
            sb_append(&sb, "]\n");

            /* Key=value pairs inside the section */
            pln_value_t *kv = child->child;
            while (kv) {
                if (!kv->key) { kv = kv->next; continue; }
                sb_append(&sb, kv->key);
                sb_append(&sb, "=");
                write_value(&sb, kv);
                sb_append_char(&sb, '\n');
                kv = kv->next;
            }
        } else {
            /* Root-level key=value (not inside any section) */
            sb_append(&sb, child->key);
            sb_append(&sb, "=");
            write_value(&sb, child);
            sb_append_char(&sb, '\n');
        }

        child = child->next;
    }

    return sb.buf;  /* caller takes ownership */
}
