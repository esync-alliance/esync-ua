/*
 * xml.h
 */

#ifndef UA_XML_H_
#define UA_XML_H_

#include <libxml/tree.h>
#include <libxml/parser.h>
#include "delta.h"
#include "handler.h"

#define XMLT (xmlChar*)

int parse_diff_manifest(char* xmlFile, diff_info_t** diffInfo);
int parse_pkg_manifest(char* xmlFile, pkg_file_t** pkgFile);
int add_pkg_file_manifest(char* xmlFile, pkg_file_t* pkgFile);
int get_pkg_file_manifest(char* xmlFile, char* version, pkg_file_t* pkgFile);
int remove_old_backup(char* xmlFile, char* version);
void free_pkg_file(pkg_file_t* pkgFile);

#endif /* UA_XML_H_ */
