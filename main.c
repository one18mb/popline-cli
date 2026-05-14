/* popline-cli — `pln` command: multi-format converter & validator
 * Parsing: uses SAX interface (popline_sax.c) — no intermediate DOM.
 * From-format: DOM-based (external parsers produce pln_value_t). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popline.h"
#include "popline_sax.h"
#include "sax_formats.h"
#include "fmt_ini.h"
#include "fmt_yaml.h"
#include "fmt_toml.h"
#include "fmt_json.h"
#if __has_include(<expat.h>)
  #include "fmt_xml.h"
  #define HAVE_XML 1
#endif

/* ─── File I/O ───────────────────────────────────────────── */

static char *read_file(const char *path, int *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    *len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(*len + 1);
    if (fread(buf, 1, *len, f) != *len) { fprintf(stderr, "error: read failed\n"); exit(1); }
    fclose(f);
    buf[*len] = '\0';
    return buf;
}

static void write_file(const char *path, const char *data, int len) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "error: cannot write %s\n", path); exit(1); }
    fwrite(data, 1, len, f);
    fclose(f);
}

/* ─── pln to <format> <in.pln> <out.ext> ───────────────── */
/* Uses SAX-based converters (zero DOM, single pass) */

static void cmd_to(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: pln to <format> <in.pln> <out.ext>\n");
        exit(1);
    }
    const char *fmt   = argv[0];
    const char *inpath  = argv[1];
    const char *outpath = argv[2];

    int len;
    char *data = read_file(inpath, &len);

    char *out = NULL;
    if (strcmp(fmt, "json") == 0) {
        out = sax_to_json(data);
    } else if (strcmp(fmt, "yaml") == 0) {
        out = sax_to_yaml(data);
    } else if (strcmp(fmt, "toml") == 0) {
        out = sax_to_toml(data);
    } else if (strcmp(fmt, "ini") == 0) {
        out = sax_to_ini(data);
    } else if (strcmp(fmt, "xml") == 0) {
#ifdef HAVE_XML
        out = sax_to_xml(data);
#else
        fprintf(stderr, "error: XML not available on this platform\n");
        free(data); exit(1);
#endif
    } else {
        fprintf(stderr, "error: unknown format '%s'\n", fmt);
        fprintf(stderr, "supported formats: json, yaml, toml, ini, xml\n");
        free(data);
        exit(1);
    }

    free(data);
    if (!out) { fprintf(stderr, "error: conversion failed\n"); exit(1); }
    write_file(outpath, out, (int)strlen(out));
    free(out);
    printf("converted %s → %s\n", inpath, outpath);
}

/* ─── pln from <format> <in.ext> <out.pln> ─────────────── */

static void cmd_from(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: pln from <format> <in.ext> <out.pln>\n");
        exit(1);
    }
    const char *fmt   = argv[0];
    const char *inpath  = argv[1];
    const char *outpath = argv[2];

    int len;
    char *data = read_file(inpath, &len);

    pln_value_t *v = NULL;
    if (strcmp(fmt, "json") == 0) {
        v = fmt_json_parse(data);
    } else if (strcmp(fmt, "yaml") == 0) {
        v = fmt_yaml_parse(data);
    } else if (strcmp(fmt, "toml") == 0) {
        v = fmt_toml_parse(data);
    } else if (strcmp(fmt, "ini") == 0) {
        v = fmt_ini_parse(data);
    } else if (strcmp(fmt, "xml") == 0) {
#ifdef HAVE_XML
        v = fmt_xml_parse(data);
#else
        fprintf(stderr, "error: XML not available on this platform\n");
        free(data); exit(1);
#endif
    } else {
        fprintf(stderr, "error: unknown format '%s'\n", fmt);
        fprintf(stderr, "supported formats: json, yaml, toml, ini, xml\n");
        free(data);
        exit(1);
    }

    free(data);
    if (!v) { fprintf(stderr, "error: conversion failed\n"); exit(1); }

    char *out = pln_dumps(v);
    if (!out) { fprintf(stderr, "error: PopLine serialization failed\n"); exit(1); }
    write_file(outpath, out, (int)strlen(out));
    free(out);
    pln_value_free(v);
    printf("converted %s → %s\n", inpath, outpath);
}

/* ─── pln validate <file> ────────────────────────────────── */
/* Uses SAX parser (no DOM allocated) */

static int noop_cb(const pln_sax_ev_t *ev, void *user) { (void)ev; (void)user; return 0; }

static void cmd_validate(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "usage: pln validate <file>\n");
        exit(1);
    }
    int len;
    char *data = read_file(argv[0], &len);
    int ok = (pln_sax_parse(data, noop_cb, NULL) == 0);
    free(data);
    if (!ok) { fprintf(stderr, "invalid\n"); exit(1); }
    printf("valid\n");
}

/* ─── Help ────────────────────────────────────────────────── */

static void usage(void) {
    printf(
        "pln — PopLine command-line tool\n"
        "\n"
        "Usage:\n"
        "  pln to <format> <in.pln> <out.ext>       PopLine → format\n"
        "  pln from <format> <in.ext> <out.pln>     format → PopLine\n"
        "  pln validate <file>                      Validate a PopLine file\n"
        "  pln help                                 Show this help\n"
        "\n"
        "Supported formats:\n"
        "  json   (fully supported)\n"
        "  yaml   (fully supported)\n"
        "  toml   (fully supported)\n"
        "  ini    (fully supported)\n"
        "  xml    (fully supported, requires expat)\n"
        "\n"
        "Examples:\n"
        "  pln to json config.pln config.json         PopLine → JSON\n"
        "  pln from json test.json test.pln     JSON → PopLine\n"
        "  pln validate schema.pln                    Validate only\n"
    );
}

/* ─── Entry point ────────────────────────────────────────── */

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }

    if (strcmp(argv[1], "to") == 0)
        cmd_to(argc - 2, argv + 2);
    else if (strcmp(argv[1], "from") == 0)
        cmd_from(argc - 2, argv + 2);
    else if (strcmp(argv[1], "validate") == 0)
        cmd_validate(argc - 2, argv + 2);
    else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
        usage();
    else {
        fprintf(stderr, "unknown command: %s\n\n", argv[1]);
        usage();
        return 1;
    }
    return 0;
}
