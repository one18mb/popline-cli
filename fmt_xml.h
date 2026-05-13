/* fmt_xml.h — XML ↔ PopLine conversion declarations (stub) */
#ifndef FMT_XML_H
#define FMT_XML_H

#include "popline.h"

pln_value_t *fmt_xml_parse(const char *text);    /* XML text → PopLine DOM */
char       *fmt_xml_dumps(pln_value_t *v);       /* PopLine DOM → XML text */

#endif
