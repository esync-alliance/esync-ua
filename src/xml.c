/*
 * xml.c
 */

#include "xml.h"

static xmlNodePtr get_xml_child(xmlNodePtr parent, xmlChar * name);
static diff_info_t* get_xml_diff_info(xmlNodePtr ptr);
static pkg_data_t* get_xml_pkg_info(xmlNodePtr ptr);
static pkg_data_t* get_pkg_data_xml(xmlNodePtr root, char * version);


static xmlNodePtr get_xml_child(xmlNodePtr parent, xmlChar * name) {

    xmlNodePtr c = NULL;

    if (!parent) { return 0; }

    for (c = xmlFirstElementChild(parent); c; c = xmlNextElementSibling(c)) {

        if (c->type == XML_ELEMENT_NODE && xmlStrEqual(name, c->name)) {
            return c;
        }

    }

    return 0;
}


static diff_info_t* get_xml_diff_info(xmlNodePtr ptr) {

    xmlChar * c;
    xmlNodePtr n, p;

    if (!ptr) { return 0; }

    diff_info_t * diffInfo = f_malloc(sizeof(diff_info_t));

    for (n = xmlFirstElementChild(ptr); n; n = xmlNextElementSibling(n)) {

        if (n->type != XML_ELEMENT_NODE) { continue; }

        if (xmlStrEqual(n->name, XMLT "name")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!diffInfo->name) {
                    diffInfo->name = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "sha256")) {

            if ((p = get_xml_child(n, "old"))) {
                if ((c = xmlNodeGetContent(p))) {
                    if(!*diffInfo->sha256.old && (strlen((const char *)c) == (SHA256_STRING_LENGTH -1))) {
                        strcpy((char *)diffInfo->sha256.old, (const char *)c);
                    }
                    xmlFree(c);
                }
            }

            if ((p = get_xml_child(n, "new"))) {
                if ((c = xmlNodeGetContent(p))) {
                    if(!*diffInfo->sha256.new && (strlen((const char *)c) == (SHA256_STRING_LENGTH -1))) {
                        strcpy((char *)diffInfo->sha256.new, (const char *)c);
                    }
                    xmlFree(c);
                }
            }

        } else if (xmlStrEqual(n->name, XMLT "format")) {

            if ((c = xmlNodeGetContent(n))) {
                diffInfo->format = diff_format_enum((const char *)c);
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "compression")) {

            if ((c = xmlNodeGetContent(n))) {
                diffInfo->compression = diff_compression_enum((const char *)c);
                xmlFree(c);
            }
        }
    }

    if (!S(diffInfo->name) || (diffInfo->format == 0) || (diffInfo->compression == 0)) {
        DBG("Incomplete diff node");
        if (diffInfo->name) free(diffInfo->name);
        free(diffInfo);
        diffInfo = NULL;
    }

    return diffInfo;
}


int parse_diff_manifest(char * xmlFile, diff_info_t ** diffInfo) {

#define TYPEQL(a,b,e) ({typ = e; xmlStrEqual(XMLT a, XMLT b);})

    int err = E_UA_OK;
    diff_type_t typ;
    diff_info_t * di;
    xmlDocPtr doc = NULL;
    xmlNodePtr root, node = NULL;

    DBG("parsing diff manifest: %s", xmlFile);

    do {

        BOLT_IF(!xmlFile || access(xmlFile, R_OK), E_UA_ERR, "diff manifest not available %s", xmlFile);
        BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
        root = xmlDocGetRootElement(doc);

        for (node = xmlFirstElementChild(root); node; node = xmlNextElementSibling(node)) {

            if (node->type != XML_ELEMENT_NODE) { continue; }

            if (TYPEQL(node->name, "added", DT_ADDED) || TYPEQL(node->name, "removed", DT_REMOVED)
                    || TYPEQL(node->name, "unchanged", DT_UNCHANGED) || TYPEQL(node->name, "changed", DT_CHANGED)) {

                if ((di = get_xml_diff_info(get_xml_child(node, "file")))) {
                    di->type = typ;
                    DL_APPEND(*diffInfo, di);
                }
            }
        }

    } while (0);

    if (doc) {
        xmlFreeDoc(doc);
        xmlCleanupParser();
    }

    return err;
}


static pkg_data_t* get_xml_pkg_info(xmlNodePtr ptr) {

    xmlChar * c;
    xmlNodePtr n;

    if (!ptr) { return 0; }

    pkg_data_t * pkgData = f_malloc(sizeof(pkg_data_t));

    for (n = xmlFirstElementChild(ptr); n; n = xmlNextElementSibling(n)) {

        if (n->type != XML_ELEMENT_NODE) { continue; }

        if (xmlStrEqual(n->name, XMLT "type")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!pkgData->type) {
                    pkgData->type = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "sha256")) {

            if ((c = xmlNodeGetContent(n))) {
                if(!*pkgData->sha256 && (strlen((const char *)c) == (SHA256_STRING_LENGTH -1))) {
                    strcpy((char *)pkgData->sha256, (const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "name")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!pkgData->name) {
                    pkgData->name = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "version")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!pkgData->version) {
                    pkgData->version = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "file")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!pkgData->file) {
                    pkgData->file = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "downloaded")) {

            if ((c = xmlNodeGetContent(n))) {
                pkgData->downloaded = atoi((const char *)c);
                xmlFree(c);
            }

        }
    }

    if (!S(pkgData->name) || !S(pkgData->type) || !S(pkgData->file) || !S(pkgData->version) || !S(pkgData->sha256)) {
        DBG("Incomplete pkg node");
        if (pkgData->name) free(pkgData->name);
        if (pkgData->type) free(pkgData->type);
        if (pkgData->file) free(pkgData->file);
        if (pkgData->version) free(pkgData->version);
        free(pkgData);
        pkgData = NULL;
    }

    return pkgData;
}


int parse_pkg_manifest(char * xmlFile, pkg_data_t ** pkgData) {

    int err = E_UA_OK;
    pkg_data_t * pd;
    xmlDocPtr doc = NULL;
    xmlNodePtr root, node = NULL;

    DBG("parsing pkg manifest: %s", xmlFile);

    do {

        BOLT_IF(!xmlFile || access(xmlFile, R_OK), E_UA_ERR, "pkg manifest not available %s", xmlFile);
        BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
        root = xmlDocGetRootElement(doc);

        for (node = xmlFirstElementChild(root); node; node = xmlNextElementSibling(node)) {

            if (node->type != XML_ELEMENT_NODE) { continue; }

            if (xmlStrEqual(node->name, XMLT "package")) {
                if ((pd = get_xml_pkg_info(node)))
                    DL_APPEND(*pkgData, pd);
            }
        }

    } while (0);

    if (doc) {
        xmlFreeDoc(doc);
        xmlCleanupParser();
    }

    return err;
}


int add_pkg_data_manifest(char * xmlFile, pkg_data_t * pkgData) {

    int err = E_UA_OK;
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL, node = NULL;

    do {

        if (!access(xmlFile, W_OK)) {
            BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
            root = xmlDocGetRootElement(doc);
        } else {
            doc = xmlNewDoc("1.0");
            root = xmlNewNode(NULL, XMLT "manifest_pkg");
            xmlDocSetRootElement(doc, root);
        }

        pkg_data_t * pd, * aux;
        DL_FOREACH(pkgData, pd) {

            if ((aux = get_pkg_data_xml(root, pd->version))) { free(aux); continue; }

            DBG("Adding pkg entry for version: %s in %s", pd->version, xmlFile);

            node = xmlNewChild(root, NULL, XMLT "package", NULL);
            xmlNewChild(node, NULL, XMLT "type", XMLT pd->type);
            xmlNewChild(node, NULL, XMLT "name", XMLT pd->name);
            xmlNewChild(node, NULL, XMLT "version", XMLT pd->version);
            xmlNewChild(node, NULL, XMLT "sha256", XMLT pd->sha256);
            xmlNewChild(node, NULL, XMLT "file", XMLT pd->file);
            xmlNewChild(node, NULL, XMLT "downloaded", XMLT (pd->downloaded? "1":"0"));

        }
        BOLT_IF((xmlSaveFormatFileEnc(xmlFile, doc, "UTF-8", 1) < 0), E_UA_ERR, "failed to save pkg manifest");

    } while (0);

    if (doc) {
        xmlFreeDoc(doc);
        xmlCleanupParser();
    }

    return err;
}


static pkg_data_t* get_pkg_data_xml(xmlNodePtr root, char * version) {

    xmlChar * c;
    xmlNodePtr node, n;
    pkg_data_t * pkgData = NULL;

    for (node = xmlFirstElementChild(root); node; node = xmlNextElementSibling(node)) {

        if ((node->type == XML_ELEMENT_NODE) && xmlStrEqual(node->name, XMLT "package")) {
            if ((n = get_xml_child(node, XMLT "version"))) {
                if ((c = xmlNodeGetContent(n))) {
                    if (xmlStrEqual(c, XMLT version)) {
                        pkgData = get_xml_pkg_info(node);
                    }
                    xmlFree(c);
                }
            }
        }

        if (pkgData) { break; }
    }

    return pkgData;
}


pkg_data_t* get_pkg_data_manifest(char * xmlFile, char * version) {

    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL;
    pkg_data_t * pkgData = NULL;

    if (xmlFile && !access(xmlFile, R_OK)) {
        if ((doc = xmlReadFile(xmlFile, NULL, 0))) {
            root = xmlDocGetRootElement(doc);
            pkgData = get_pkg_data_xml(root, version);
        } else {
            DBG_SYS("Could not read xml file %s", xmlFile);
        }
    } else {
        DBG("pkg manifest not available %s", xmlFile);
    }

    return pkgData;
}
