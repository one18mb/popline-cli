/* popline-cli — `pln` command: JSON ↔ PopLine converter & validator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popline.h"
#include "cjson/cJSON.h"

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

static int has_ext(const char *path, const char *ext) {
    int plen = (int)strlen(path), elen = (int)strlen(ext);
    return plen >= elen && strcmp(path + plen - elen, ext) == 0;
}

static void cmd_convert(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: pln convert <input> <output>\n"); exit(1);
    }
    const char *inpath = argv[0], *outpath = argv[1];
    int len;
    char *data = read_file(inpath, &len);

    if (has_ext(inpath, ".json")) {
        /* JSON → PopLine */
        cJSON *cj = cJSON_Parse(data);
        if (!cj) { fprintf(stderr, "error: invalid JSON\n"); exit(1); }
        pln_value_t *v = pln_loads_json(data);
        cJSON_Delete(cj);
        if (!v) { fprintf(stderr, "error: JSON conversion failed\n"); exit(1); }
        char *out = pln_dumps(v);
        write_file(outpath, out, (int)strlen(out));
        free(out);
        pln_value_free(v);
    } else {
        /* PopLine → JSON (default) */
        pln_value_t *v = pln_loads(data);
        if (!v) { fprintf(stderr, "error: invalid PopLine\n"); exit(1); }
        char *out = pln_dumps_json(v);
        if (!out) { fprintf(stderr, "error: JSON conversion failed\n"); exit(1); }
        int olen = (int)strlen(out);
        out[olen] = '\n';
        write_file(outpath, out, olen + 1);
        free(out);
        pln_value_free(v);
    }
    free(data);
    printf("converted %s → %s\n", inpath, outpath);
}

static void cmd_validate(int argc, char **argv) {
    if (argc < 1) { fprintf(stderr, "usage: pln validate <file>\n"); exit(1); }
    int len;
    char *data = read_file(argv[0], &len);
    pln_value_t *v = pln_loads(data);
    free(data);
    if (!v) { fprintf(stderr, "invalid\n"); exit(1); }
    pln_value_free(v);
    printf("valid\n");
}

static void usage(void) {
    printf(
        "pln — PopLine command-line tool\n"
        "\n"
        "Usage:\n"
        "  pln convert <input> <output>   Convert between JSON and PopLine\n"
        "  pln validate <file>            Validate a PopLine file\n"
        "  pln help                       Show this help\n"
        "\n"
        "Examples:\n"
        "  pln convert package.json package.pln    JSON → PopLine\n"
        "  pln convert config.pln config.json      PopLine → JSON\n"
        "  pln validate schema.pln                 Validate only\n"
        "\n"
        "Extension rules:\n"
        "  .json → .pln   JSON to PopLine\n"
        "  .pln  → .json  PopLine to JSON\n"
    );
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }

    if (strcmp(argv[1], "convert") == 0)
        cmd_convert(argc - 2, argv + 2);
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
