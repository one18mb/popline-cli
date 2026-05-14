/* fmt_yaml.c — Minimal YAML ↔ PopLine DOM conversion */
#include "fmt_yaml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ─── YAML Parser ────────────────────────────────────────────── */
/* Simplified indentation-based YAML subset: mappings and sequences */

static int yaml_indent(const char *line, int len) {
    int n = 0;
    while (n < len && line[n] == ' ') n++;
    return n;
}

static char *yaml_trim(const char *s, int len) {
    int start = 0, end = len;
    while (start < end && (s[start] == ' ' || s[start] == '\t')) start++;
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t')) end--;
    char *r = (char *)malloc(end - start + 1);
    memcpy(r, s + start, end - start);
    r[end - start] = '\0';
    return r;
}

static pln_value_t *yaml_parse_scalar(const char *s) {
    if (!s || !*s) return pln_value_new_string("");
    if (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "on") == 0) return pln_value_new_bool(1);
    if (strcmp(s, "false") == 0 || strcmp(s, "no") == 0 || strcmp(s, "off") == 0) return pln_value_new_bool(0);
    if (strcmp(s, "null") == 0 || strcmp(s, "~") == 0) return pln_value_new_null();
    if (s[0] == '-' || isdigit(s[0])) {
        int has_dot = 0, has_e = 0;
        for (int i = 0; s[i]; i++) { if (s[i] == '.') has_dot = 1; if (s[i] == 'e' || s[i] == 'E') has_e = 1; }
        char *end;
        if (has_dot || has_e) {
            errno = 0; double d = strtod(s, &end);
            if (*end == '\0' && errno != ERANGE) return pln_value_new_float(d);
        } else {
            errno = 0; long long ll = strtoll(s, &end, 10);
            if (*end == '\0' && errno != ERANGE) return pln_value_new_int(ll);
        }
    }
    /* Strip quotes if present */
    int slen = (int)strlen(s);
    if (slen >= 2 && ((s[0] == '"' && s[slen-1] == '"') || (s[0] == '\'' && s[slen-1] == '\''))) {
        char *inner = (char *)malloc(slen - 1);
        memcpy(inner, s + 1, slen - 2);
        inner[slen - 2] = '\0';
        pln_value_t *v = pln_value_new_string(inner);
        free(inner);
        return v;
    }
    return pln_value_new_string(s);
}

typedef struct {
    int *indents;
    pln_value_t **containers;
    int len, cap;
} yaml_stack_t;

static void yaml_push(yaml_stack_t *s, int indent, pln_value_t *c) {
    if (s->len >= s->cap) { s->cap = s->cap ? s->cap * 2 : 16; s->indents = (int *)realloc(s->indents, s->cap * sizeof(int));
        s->containers = (pln_value_t **)realloc(s->containers, s->cap * sizeof(pln_value_t *)); }
    s->indents[s->len] = indent;
    s->containers[s->len] = c;
    s->len++;
}

static void yaml_pop_to(yaml_stack_t *s, int target_len) {
    while (s->len > target_len) s->len--;
}

pln_value_t *fmt_yaml_parse(const char *text) {
    pln_value_t *root = pln_value_new_object();
    yaml_stack_t stack = {0};
    yaml_push(&stack, -1, root);

    const char *s = text;
    while (*s) {
        const char *nl = strchr(s, '\n');
        int line_len = nl ? (int)(nl - s) : (int)strlen(s);
        const char *line = s;

        if (line_len > 0 && line[line_len-1] == '\r') line_len--;

        /* Skip empty and comment lines */
        int is_empty = 1;
        for (int i = 0; i < line_len; i++) {
            if (line[i] == '#') break;
            if (line[i] != ' ' && line[i] != '\t') { is_empty = 0; break; }
        }
        if (is_empty) { s = nl ? nl + 1 : s + line_len; continue; }

        /* Strip comments */
        int content_len = line_len;
        for (int i = 0; i < content_len; i++) {
            if (line[i] == '#' && (i == 0 || line[i-1] == ' ')) { content_len = i; break; }
        }

        int indent = yaml_indent(line, content_len);

        /* Pop stack to correct level */
        while (stack.len > 1 && indent <= stack.indents[stack.len - 1]) stack.len--;

        pln_value_t *cur = stack.containers[stack.len - 1];

        /* Sequence item: "- value" */
        if (content_len > indent + 1 && line[indent] == '-' && line[indent+1] == ' ') {
            /* Ensure parent is an array */
            pln_value_t *arr = NULL;
            if (cur->type != PLN_ARRAY) {
                /* Need to replace cur with array containing cur */
                /* For simplicity, create array and add cur as first element */
                arr = pln_value_new_array();
                /* This changes structure, better approach: track separately */
            }
            if (cur->type != PLN_ARRAY) {
                arr = pln_value_new_array();
                /* If cur is root object, add array directly */
                if (cur == root) {
                    /* Can't replace root, so add to root */
                }
            }
            if (cur->type == PLN_ARRAY) arr = cur;

            int val_start = indent + 2;
            char *val_str = yaml_trim(line + val_start, content_len - val_start);
            pln_value_t *val = yaml_parse_scalar(val_str);

            /* Check for nested mapping on same line */
            if (val_str && strchr(val_str, ':')) {
                char *colon = strchr(val_str, ':');
                *colon = '\0';
                char *k = val_str;
                char *v = colon + 1;
                while (*v == ' ') v++;
                pln_value_t *obj = pln_value_new_object();
                pln_value_t *vv = yaml_parse_scalar(v);
                pln_value_add_to_object(obj, k, vv);
                pln_value_free(val);
                if (arr) pln_value_add_to_array(arr, obj);
                yaml_push(&stack, indent, obj);
            } else {
                if (arr) pln_value_add_to_array(arr, val);
                else pln_value_free(val);
            }
            /* If arr was just created, we need to add it */
            if (arr && cur != arr) {
                /* This means arr was newly created */
            }
            free(val_str);
        }
        /* Mapping: "key: value" */
        else {
            char *colon = (char *)memchr(line + indent, ':', content_len - indent);
            if (colon) {
                int klen = (int)(colon - (line + indent));
                char *key = yaml_trim(line + indent, klen);
                int vstart = (int)(colon - line) + 1;
                int vlen = content_len - vstart;

                /* Trim value */
                char *val_str = NULL;
                if (vlen > 0) val_str = yaml_trim(line + vstart, vlen);

                if (val_str && *val_str) {
                    pln_value_t *val = yaml_parse_scalar(val_str);
                    if (cur->type == PLN_OBJECT) pln_value_add_to_object(cur, key, val);
                    else pln_value_free(val);
                } else {
                    /* Empty value — nested content follows */
                    pln_value_t *obj = pln_value_new_object();
                    if (cur->type == PLN_OBJECT) pln_value_add_to_object(cur, key, obj);
                    yaml_push(&stack, indent, obj);
                }
                free(key);
                free(val_str);
            }
        }

        s = nl ? nl + 1 : s + line_len;
    }

    free(stack.indents);
    free(stack.containers);
    return root;
}
