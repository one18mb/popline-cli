/* fmt_toml.c — Minimal TOML ↔ PopLine DOM conversion */
#include "fmt_toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ─── TOML Parser ───────────────────────────────────────────── */

typedef struct {
    const char *s;
    int pos, len;
    char error[256];
    pln_value_t *root;
    pln_value_t *cur;       /* current table */
} toml_ctx_t;

static void skip_space(toml_ctx_t *c) {
    while (c->pos < c->len && (c->s[c->pos] == ' ' || c->s[c->pos] == '\t')) c->pos++;
}

static int peek(toml_ctx_t *c) { return c->pos < c->len ? c->s[c->pos] : EOF; }

static void advance(toml_ctx_t *c) { if (c->pos < c->len) c->pos++; }

/* Parse a bare key or dotted key segment */
static char *parse_key(toml_ctx_t *c) {
    int start = c->pos;
    while (c->pos < c->len && (isalnum(c->s[c->pos]) || c->s[c->pos] == '_' || c->s[c->pos] == '-'))
        c->pos++;
    if (c->pos == start) return NULL;
    char *k = (char *)malloc(c->pos - start + 1);
    memcpy(k, c->s + start, c->pos - start);
    k[c->pos - start] = '\0';
    return k;
}

/* Navigate to dotted key path, creating objects as needed */
static pln_value_t *ensure_path(toml_ctx_t *c, const char *path) {
    if (!path || !*path) return c->cur;
    char *dup = strdup(path);
    pln_value_t *obj = c->cur;
    char *save;
    for (char *tok = strtok_r(dup, ".", &save); tok; tok = strtok_r(NULL, ".", &save)) {
        pln_value_t *child = obj->child;
        pln_value_t *found = NULL;
        while (child) {
            if (child->key && strcmp(child->key, tok) == 0) { found = child; break; }
            child = child->next;
        }
        if (!found) {
            found = pln_value_new_object();
            pln_value_add_to_object(obj, tok, found);
        }
        obj = found;
    }
    free(dup);
    return obj;
}

static char *parse_string(toml_ctx_t *c) {
    /* Only double-quoted strings */
    if (peek(c) != '"') return NULL;
    advance(c); /* skip " */
    int cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    while (c->pos < c->len) {
        int ch = peek(c);
        if (ch == '"') { advance(c); break; }
        if (ch == '\\') {
            advance(c);
            switch (peek(c)) {
            case 'n': buf[len++] = '\n'; break;
            case 't': buf[len++] = '\t'; break;
            case '"': buf[len++] = '"'; break;
            case '\\': buf[len++] = '\\'; break;
            default: buf[len++] = '\\'; buf[len++] = peek(c); break;
            }
            advance(c);
        } else {
            if (ch == '\n') { free(buf); return NULL; } /* unclosed */
            buf[len++] = ch;
            advance(c);
        }
        if (len >= cap - 1) { cap *= 2; buf = (char *)realloc(buf, cap); }
    }
    buf[len] = '\0';
    return buf;
}

static pln_value_t *parse_toml_value(toml_ctx_t *c) {
    skip_space(c);
    int ch = peek(c);
    if (ch == '"') {
        char *s = parse_string(c);
        if (!s) return NULL;
        pln_value_t *v = pln_value_new_string(s);
        free(s); return v;
    }
    if (ch == 't' && c->pos + 4 <= c->len && memcmp(c->s + c->pos, "true", 4) == 0) {
        c->pos += 4; return pln_value_new_bool(1);
    }
    if (ch == 'f' && c->pos + 5 <= c->len && memcmp(c->s + c->pos, "false", 5) == 0) {
        c->pos += 5; return pln_value_new_bool(0);
    }
    if (ch == 'n' && c->pos + 4 <= c->len && memcmp(c->s + c->pos, "null", 4) == 0) {
        c->pos += 4; return pln_value_new_null();
    }
    if (ch == '-' || ch == '+' || isdigit(ch)) {
        int start = c->pos;
        if (ch == '-' || ch == '+') { advance(c); skip_space(c); }
        while (c->pos < c->len && (isdigit(c->s[c->pos]) || c->s[c->pos] == '.' ||
               c->s[c->pos] == 'e' || c->s[c->pos] == 'E' || c->s[c->pos] == '_')) c->pos++;
        char tmp[128];
        int nlen = c->pos - start;
        if (nlen >= (int)sizeof(tmp)) return NULL;
        memcpy(tmp, c->s + start, nlen); tmp[nlen] = '\0';
        /* Remove underscores for parsing */
        int j = 0;
        for (int i = 0; i < nlen; i++) if (tmp[i] != '_') tmp[j++] = tmp[i];
        tmp[j] = '\0';

        int is_float = 0;
        for (int i = 0; i < j; i++) if (tmp[i] == '.' || tmp[i] == 'e' || tmp[i] == 'E') { is_float = 1; break; }
        char *end;
        errno = 0;
        if (is_float) {
            double d = strtod(tmp, &end);
            if (*end == '\0' && errno != ERANGE) return pln_value_new_float(d);
        } else {
            long long ll = strtoll(tmp, &end, 10);
            if (*end == '\0' && errno != ERANGE) return pln_value_new_int(ll);
        }
        return NULL;
    }
    return NULL;
}

pln_value_t *fmt_toml_parse(const char *text) {
    toml_ctx_t c;
    memset(&c, 0, sizeof(c));
    c.s = text;
    c.len = (int)strlen(text);
    c.root = pln_value_new_object();
    c.cur = c.root;

    while (c.pos < c.len) {
        int line_start = c.pos;
        while (c.pos < c.len && c.s[c.pos] != '\n') c.pos++;
        if (c.pos > line_start) {
            int line_len = c.pos - line_start;
            const char *line = c.s + line_start;
            if (line[line_len - 1] == '\r') line_len--;
            /* Skip comments and blank */
            int i;
            for (i = 0; i < line_len && (line[i] == ' ' || line[i] == '\t'); i++);
            if (i < line_len && (line[i] == '#' || line_len == 0)) { if (c.pos < c.len) c.pos++; continue; }
            if (line_len == 0) { if (c.pos < c.len) c.pos++; continue; }

            /* Table: [section] or [[array]] */
            if (line[i] == '[') {
                int is_array = (i + 1 < line_len && line[i+1] == '[');
                int end_char = is_array ? ']' : ']';
                int close_idx = line_len - 1;
                while (close_idx > i && (line[close_idx] == ' ' || line[close_idx] == '\t')) close_idx--;
                if (is_array) {
                    if (close_idx < i + 2 || line[close_idx] != ']' || line[close_idx-1] != ']') { pln_value_free(c.root); return NULL; }
                    int open_end = close_idx - 1;
                    while (open_end > i+1 && (line[open_end] == ' ' || line[open_end] == '\t')) open_end--;
                    char *name = (char *)malloc(open_end - i - 1 + 1);
                    memcpy(name, line + i + 2, open_end - i - 1);
                    name[open_end - i - 1] = '\0';
                    /* Array of tables — append to array or create */
                    int arr_start = i + 2;
                    /* Navigate path */
                    char *dup = strdup(name);
                    pln_value_t *parent = c.root;
                    char *save;
                    char *tok = strtok_r(dup, ".", &save);
                    while (tok) {
                        char *next = strtok_r(NULL, ".", &save);
                        pln_value_t *child = parent->child;
                        pln_value_t *found = NULL;
                        while (child) {
                            if (child->key && strcmp(child->key, tok) == 0) { found = child; break; }
                            child = child->next;
                        }
                        if (next) {
                            if (!found) { found = pln_value_new_object(); pln_value_add_to_object(parent, tok, found); }
                            parent = found;
                        } else {
                            /* Last segment: find or create array */
                            if (found && found->type == PLN_ARRAY) {
                                pln_value_t *new_obj = pln_value_new_object();
                                pln_value_add_to_array(found, new_obj);
                                parent = new_obj;
                            } else {
                                pln_value_t *arr = pln_value_new_array();
                                pln_value_t *new_obj = pln_value_new_object();
                                pln_value_add_to_array(arr, new_obj);
                                /* Replace or create */
                                if (found) {
                                    /* Replace existing with array containing it + new */
                                    /* For simplicity, just add to array */
                                }
                                if (parent == c.root) {
                                    /* Find and remove old key, add array */
                                }
                                pln_value_add_to_object(parent, tok, arr);
                                parent = new_obj;
                            }
                        }
                        tok = next;
                    }
                    c.cur = parent;
                    free(dup); free(name);
                } else {
                    /* Regular table */
                    int close = i + 1;
                    while (close < line_len && line[close] != ']') close++;
                    char *name = (char *)malloc(close - i - 1 + 1);
                    memcpy(name, line + i + 1, close - i - 1);
                    name[close - i - 1] = '\0';
                    c.cur = ensure_path(&c, name);
                    free(name);
                }
            } else {
                /* key = value */
                const char *eq = (const char *)memchr(line + i, '=', line_len - i);
                if (!eq) { pln_value_free(c.root); return NULL; }
                int klen = (int)(eq - (line + i));
                /* Trim key */
                int ke = klen - 1;
                while (ke >= 0 && (line[i+ke] == ' ' || line[i+ke] == '\t')) ke--;
                char *key = (char *)malloc(ke + 2);
                memcpy(key, line + i, ke + 1);
                key[ke+1] = '\0';

                toml_ctx_t valc;
                memset(&valc, 0, sizeof(valc));
                valc.s = line;
                valc.pos = (int)(eq - line) + 1;
                valc.len = line_len;
                pln_value_t *v = parse_toml_value(&valc);
                if (!v) { free(key); pln_value_free(c.root); return NULL; }
                pln_value_add_to_object(c.cur, key, v);
                free(key);
            }
        }
        if (c.pos < c.len) c.pos++;
    }
    return c.root;
}
