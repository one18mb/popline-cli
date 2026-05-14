/* sax_formats.h — SAX-based format serializers */
#ifndef SAX_FORMATS_H
#define SAX_FORMATS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Each returns malloc'd string or NULL on error. Caller must free. */
char *sax_to_json(const char *pln_text);
char *sax_to_yaml(const char *pln_text);
char *sax_to_toml(const char *pln_text);
char *sax_to_ini(const char *pln_text);
char *sax_to_xml(const char *pln_text);

#ifdef __cplusplus
}
#endif

#endif /* SAX_FORMATS_H */
