/* fmt_xml.c — XML ↔ PopLine bidirectional conversion using expat (SAX) */
#include "fmt_xml.h"
#include <expat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
