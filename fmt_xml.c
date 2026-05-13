/* fmt_xml.c — XML ↔ PopLine bidirectional conversion using expat (SAX) */
#include "fmt_xml.h"
#include <expat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Dynamic string builder ────────────────────────────────── */

typedef struct {
    char *buf;
    int   len;
    int   cap;
} strbuf_t;

static void strbuf_init(strbuf_t *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void strbuf_append_len(strbuf_t *sb, const char *s, int len) {
    if (len <= 0) return;
    if (sb->len + len + 1 > sb->cap) {
        sb->cap = sb->cap ? sb->cap * 2 : 256;
        while (sb->len + len + 1 > sb->cap) sb->cap *= 2;
        char *new_buf = (char *)realloc(sb->buf, sb->cap);
        if (!new_buf) return;
        sb->buf = new_buf;
    }
    memcpy(sb->buf + sb->len, s, len);
    sb->len += len;
    sb->buf[sb->len] = '\0';
}

static void strbuf_append(strbuf_t *sb, const char *s) {
    if (s) strbuf_append_len(sb, s, (int)strlen(s));
}

static void strbuf_append_char(strbuf_t *sb, char c) {
    strbuf_append_len(sb, &c, 1);
}

static char *strbuf_detach(strbuf_t *sb) {
    char *result = sb->buf ? sb->buf : strdup("");
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
    return result;
}

static void strbuf_free(strbuf_t *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

/* ─── XML Escaping ──────────────────────────────────────────── */

static char *xml_escape(const char *s) {
    if (!s) return strdup("");
    int needed = 0;
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '&':  needed += 5; break;
            case '<':  needed += 4; break;
            case '>':  needed += 4; break;
            case '"':  needed += 6; break;
            case '\'': needed += 6; break;
            default:   needed += 1; break;
        }
    }
    char *out = (char *)malloc(needed + 1);
    if (!out) return NULL;
    int pos = 0;
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '&':  memcpy(out + pos, "&amp;", 5);  pos += 5; break;
            case '<':  memcpy(out + pos, "&lt;", 4);   pos += 4; break;
            case '>':  memcpy(out + pos, "&gt;", 4);   pos += 4; break;
            case '"':  memcpy(out + pos, "&quot;", 6); pos += 6; break;
            case '\'': memcpy(out + pos, "&apos;", 6); pos += 6; break;
            default:   out[pos++] = *p; break;
        }
    }
    out[pos] = '\0';
    return out;
}

/* ─── SAX Parse State ──────────────────────────────────────── */

typedef struct {
    pln_value_t **obj_stack;   /* stack of element objects being built */
    char        **text_stack;  /* parallel stack of text buffers (per element) */
    int          *text_len_stack;
    int          *text_cap_stack;
    int           stack_len;
    int           stack_cap;
    pln_value_t  *root;        /* root object (return value) */
    int           error;       /* set on OOM or internal failure */
} xml_parse_state_t;

/* ─── Stack helpers ─────────────────────────────────────────── */

static int frame_push(xml_parse_state_t *st, pln_value_t *obj) {
    if (st->stack_len >= st->stack_cap) {
        int new_cap = st->stack_cap ? st->stack_cap * 2 : 16;
        pln_value_t **new_objs  = (pln_value_t **)realloc(st->obj_stack, new_cap * sizeof(pln_value_t *));
        char         **new_texts = (char **)realloc(st->text_stack, new_cap * sizeof(char *));
        int           *new_lens  = (int *)realloc(st->text_len_stack, new_cap * sizeof(int));
        int           *new_caps  = (int *)realloc(st->text_cap_stack, new_cap * sizeof(int));
        if (!new_objs || !new_texts || !new_lens || !new_caps) {
            free(new_objs); free(new_texts); free(new_lens); free(new_caps);
            return -1;
        }
        st->obj_stack     = new_objs;
        st->text_stack    = new_texts;
        st->text_len_stack = new_lens;
        st->text_cap_stack = new_caps;
        st->stack_cap     = new_cap;
    }
    st->obj_stack[st->stack_len]      = obj;
    st->text_stack[st->stack_len]     = NULL;
    st->text_len_stack[st->stack_len] = 0;
    st->text_cap_stack[st->stack_len] = 0;
    st->stack_len++;
    return 0;
}

static void frame_pop(xml_parse_state_t *st, pln_value_t **out_obj,
                      char **out_text, int *out_text_len) {
    if (st->stack_len == 0) {
        if (out_obj)      *out_obj      = NULL;
        if (out_text)     *out_text     = NULL;
        if (out_text_len) *out_text_len = 0;
        return;
    }
    st->stack_len--;
    if (out_obj)      *out_obj      = st->obj_stack[st->stack_len];
    if (out_text)     *out_text     = st->text_stack[st->stack_len];
    if (out_text_len) *out_text_len = st->text_len_stack[st->stack_len];
}

static void frame_append_text(xml_parse_state_t *st, const char *s, int len) {
    if (st->stack_len == 0 || len <= 0) return;
    int idx = st->stack_len - 1;
    int new_len = st->text_len_stack[idx] + len;
    if (new_len + 1 > st->text_cap_stack[idx]) {
        int new_cap = st->text_cap_stack[idx] ? st->text_cap_stack[idx] * 2 : 256;
        while (new_len + 1 > new_cap) new_cap *= 2;
        char *new_buf = (char *)realloc(st->text_stack[idx], new_cap);
        if (!new_buf) return;
        st->text_stack[idx]    = new_buf;
        st->text_cap_stack[idx] = new_cap;
    }
    memcpy(st->text_stack[idx] + st->text_len_stack[idx], s, len);
    st->text_len_stack[idx] = new_len;
    st->text_stack[idx][new_len] = '\0';
}

/* ─── SAX Handlers ──────────────────────────────────────────── */

static void XMLCALL
xml_start_element(void *userData, const XML_Char *name, const XML_Char **atts) {
    xml_parse_state_t *st = (xml_parse_state_t *)userData;
    if (st->error) return;

    pln_value_t *obj = pln_value_new_object();
    if (!obj) { st->error = 1; return; }

    /* Process attributes: store with @ prefix */
    if (atts) {
        for (int i = 0; atts[i]; i += 2) {
            /* Build "@attrname" key, transferred via _nocopy */
            size_t attr_name_len = strlen(atts[i]);
            char *attr_key = (char *)malloc(attr_name_len + 2);
            if (!attr_key) { pln_value_free(obj); st->error = 1; return; }
            attr_key[0] = '@';
            memcpy(attr_key + 1, atts[i], attr_name_len + 1);
            pln_value_add_to_object_nocopy(obj, attr_key,
                pln_value_new_string(atts[i + 1] ? atts[i + 1] : ""));
        }
    }

    /* Add to parent element; handle duplicate keys by creating array */
    if (st->stack_len > 0) {
        pln_value_t *parent = st->obj_stack[st->stack_len - 1];
        /* Check if key already exists */
        pln_value_t *existing = NULL, *prev = NULL;
        for (pln_value_t *c = parent->child; c; prev = c, c = c->next) {
            if (c->key && strcmp(c->key, name) == 0) { existing = c; break; }
        }
        if (existing) {
            if (existing->type == PLN_ARRAY) {
                /* Append to existing array */
                pln_value_add_to_array(existing, obj);
            } else {
                /* Upgrade to array: [existing, obj] */
                pln_value_t *arr = pln_value_new_array();
                /* Detach existing from parent */
                if (prev) prev->next = existing->next;
                else parent->child = existing->next;
                existing->next = NULL;
                existing->key = NULL; /* key is now owned by arr context */
                pln_value_add_to_array(arr, existing);
                pln_value_add_to_array(arr, obj);
                pln_value_add_to_object(parent, name, arr);
            }
        } else {
            pln_value_add_to_object(parent, name, obj);
        }
    }

    /* Push this element onto the stack */
    if (frame_push(st, obj) != 0) {
        pln_value_free(obj);
        st->error = 1;
    }
}

static void XMLCALL
xml_end_element(void *userData, const XML_Char *name) {
    xml_parse_state_t *st = (xml_parse_state_t *)userData;
    if (st->error) return;
    if (st->stack_len == 0) { st->error = 1; return; }

    pln_value_t *obj;
    char *text;
    int text_len;
    frame_pop(st, &obj, &text, &text_len);

    /* Attach accumulated text content as #text */
    if (text && text_len > 0) {
        pln_value_add_to_object(obj, "#text", pln_value_new_string_len(text, text_len));
    }
    free(text);

    /* If the stack is now empty, this was the document element:
       add it to the root object using its tag name as key. */
    if (st->stack_len == 0) {
        pln_value_add_to_object(st->root, name, obj);
    }
}

static void XMLCALL
xml_char_data(void *userData, const XML_Char *s, int len) {
    xml_parse_state_t *st = (xml_parse_state_t *)userData;
    if (st->error || st->stack_len == 0) return;
    frame_append_text(st, s, len);
}

/* ─── Cleanup helper ────────────────────────────────────────── */

static void parse_state_free(xml_parse_state_t *st) {
    for (int i = 0; i < st->stack_len; i++)
        free(st->text_stack[i]);
    free(st->obj_stack);
    free(st->text_stack);
    free(st->text_len_stack);
    free(st->text_cap_stack);
    st->obj_stack      = NULL;
    st->text_stack     = NULL;
    st->text_len_stack = NULL;
    st->text_cap_stack = NULL;
    st->stack_len      = 0;
    st->stack_cap      = 0;
}

/* ─── XML → PopLine (public API) ─────────────────────────────── */

pln_value_t *fmt_xml_parse(const char *text) {
    if (!text) return NULL;

    XML_Parser parser = XML_ParserCreate(NULL);
    if (!parser) return NULL;

    xml_parse_state_t st;
    memset(&st, 0, sizeof(st));
    st.root = pln_value_new_object();
    if (!st.root) {
        XML_ParserFree(parser);
        return NULL;
    }

    XML_SetUserData(parser, &st);
    XML_SetElementHandler(parser, xml_start_element, xml_end_element);
    XML_SetCharacterDataHandler(parser, xml_char_data);

    int len = (int)strlen(text);
    int done = 1;
    enum XML_Status status = XML_Parse(parser, text, len, done);

    if (status != XML_STATUS_OK || st.error) {
        pln_value_free(st.root);
        parse_state_free(&st);
        XML_ParserFree(parser);
        return NULL;
    }

    XML_ParserFree(parser);
    parse_state_free(&st);
    return st.root;
}

/* ─── XML Serialization (PopLine → XML) ─────────────────────── */

static void serialize_value_as_child(strbuf_t *sb, const char *tag, pln_value_t *v);

static void serialize_object(strbuf_t *sb, const char *tag, pln_value_t *obj) {
    strbuf_t body;          /* text content (#text) */
    strbuf_t children_buf;  /* child elements */
    strbuf_t attrs;         /* attribute list, e.g. ' id="1" name="x"' */

    strbuf_init(&body);
    strbuf_init(&children_buf);
    strbuf_init(&attrs);

    /* ── First pass: separate #text, @attr, and child elements ── */
    for (pln_value_t *c = obj->child; c; c = c->next) {
        if (!c->key || !*(c->key)) continue;

        if (strcmp(c->key, "#text") == 0) {
            /* Text content: convert value to string and escape */
            char *escaped = NULL;
            switch (c->type) {
            case PLN_STRING:
                escaped = xml_escape(c->data.string_val ? c->data.string_val : "");
                break;
            case PLN_INT: {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", c->data.int_val);
                escaped = xml_escape(buf);
                break;
            }
            case PLN_FLOAT: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.17g", c->data.float_val);
                escaped = xml_escape(buf);
                break;
            }
            case PLN_BOOL:
                escaped = strdup(c->data.bool_val ? "true" : "false");
                break;
            default:
                break;
            }
            if (escaped) {
                strbuf_append(&body, escaped);
                free(escaped);
            }
        } else if (c->key[0] == '@') {
            /* Attribute: key without @ prefix, value escaped */
            const char *attr_val = "";
            char local_buf[64];
            local_buf[0] = '\0';

            switch (c->type) {
            case PLN_STRING:
                attr_val = c->data.string_val ? c->data.string_val : "";
                break;
            case PLN_INT:
                snprintf(local_buf, sizeof(local_buf), "%lld", c->data.int_val);
                attr_val = local_buf;
                break;
            case PLN_FLOAT:
                snprintf(local_buf, sizeof(local_buf), "%.17g", c->data.float_val);
                attr_val = local_buf;
                break;
            case PLN_BOOL:
                attr_val = c->data.bool_val ? "true" : "false";
                break;
            default:
                break;
            }

            char *escaped = xml_escape(attr_val);
            if (escaped) {
                strbuf_append_char(&attrs, ' ');
                strbuf_append(&attrs, c->key + 1);  /* skip @ */
                strbuf_append(&attrs, "=\"");
                strbuf_append(&attrs, escaped);
                strbuf_append_char(&attrs, '"');
                free(escaped);
            }
        } else if (c->type == PLN_ARRAY) {
            /* Array: each element becomes a sibling tag with this key */
            for (pln_value_t *elem = c->child; elem; elem = elem->next)
                serialize_value_as_child(&children_buf, c->key, elem);
        } else {
            /* Regular child element */
            serialize_value_as_child(&children_buf, c->key, c);
        }
    }

    /* ── Write the XML element ── */
    strbuf_append_char(sb, '<');
    strbuf_append(sb, tag);
    strbuf_append_len(sb, attrs.buf, attrs.len);

    if (body.len == 0 && children_buf.len == 0) {
        /* Empty element: self-closing tag */
        strbuf_append(sb, "/>");
    } else {
        strbuf_append_char(sb, '>');
        strbuf_append_len(sb, body.buf, body.len);
        strbuf_append_len(sb, children_buf.buf, children_buf.len);
        strbuf_append(sb, "</");
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
    }

    strbuf_free(&body);
    strbuf_free(&children_buf);
    strbuf_free(&attrs);
}

static void serialize_value_as_child(strbuf_t *sb, const char *tag, pln_value_t *v) {
    if (!tag || !(*tag)) return;
    if (!v || v->type == PLN_NULL) {
        /* Null or missing value: self-closing tag */
        strbuf_append_char(sb, '<');
        strbuf_append(sb, tag);
        strbuf_append(sb, "/>");
        return;
    }

    switch (v->type) {
    case PLN_OBJECT:
        serialize_object(sb, tag, v);
        break;

    case PLN_ARRAY:
        for (pln_value_t *c = v->child; c; c = c->next)
            serialize_value_as_child(sb, tag, c);
        break;

    case PLN_STRING: {
        char *escaped = xml_escape(v->data.string_val ? v->data.string_val : "");
        strbuf_append_char(sb, '<');
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        strbuf_append(sb, escaped);
        strbuf_append(sb, "</");
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        free(escaped);
        break;
    }

    case PLN_INT: {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", v->data.int_val);
        char *escaped = xml_escape(buf);
        strbuf_append_char(sb, '<');
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        strbuf_append(sb, escaped);
        strbuf_append(sb, "</");
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        free(escaped);
        break;
    }

    case PLN_FLOAT: {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", v->data.float_val);
        char *escaped = xml_escape(buf);
        strbuf_append_char(sb, '<');
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        strbuf_append(sb, escaped);
        strbuf_append(sb, "</");
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        free(escaped);
        break;
    }

    case PLN_BOOL:
        strbuf_append_char(sb, '<');
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        strbuf_append(sb, v->data.bool_val ? "true" : "false");
        strbuf_append(sb, "</");
        strbuf_append(sb, tag);
        strbuf_append_char(sb, '>');
        break;

    default:
        /* PLN_NULL already handled at top; unsupported types become self-closing */
        strbuf_append_char(sb, '<');
        strbuf_append(sb, tag);
        strbuf_append(sb, "/>");
        break;
    }
}

/* ─── PopLine → XML (public API) ─────────────────────────────── */

char *fmt_xml_dumps(pln_value_t *v) {
    if (!v || v->type != PLN_OBJECT) return NULL;

    strbuf_t sb;
    strbuf_init(&sb);

    for (pln_value_t *c = v->child; c; c = c->next) {
        if (c->key)
            serialize_value_as_child(&sb, c->key, c);
    }

    return strbuf_detach(&sb);
}
