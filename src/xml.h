/*
 * xml.h
 */

#ifndef _UA_XML_H_
#define _UA_XML_H_

#include "common.h"
#include <libxml/tree.h>
#include <libxml/parser.h>
#include "delta.h"

#define XMLT (xmlChar*)

int parse_diff_manifest(char * xmlFile, diff_info_t ** diffInfo);
int parse_pkg_manifest(char * xmlFile, pkg_data_t ** pkgData);
int add_pkg_data_manifest(char * xmlFile, pkg_data_t * pkgData);
pkg_data_t* get_pkg_data_manifest(char * xmlFile, char * version);

#endif /* _UA_XML_H_ */
