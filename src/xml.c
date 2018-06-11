/*
 * xml.c
 */

#include "xml.h"

static xmlNodePtr get_xml_child(xmlNodePtr parent, xmlChar * name);
static diff_info_t* get_xml_diff_info(xmlNodePtr ptr);
static pkg_file_t* get_xml_pkg_file(xmlNodePtr ptr);
static pkg_file_t* get_xml_version_pkg_file(xmlNodePtr root, char * version);


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
                    if(!*diffInfo->sha256.old && (strlen((const char *)c) == (SHA256_HEX_LENGTH - 1))) {
                        strcpy(diffInfo->sha256.old, (const char *)c);
                    }
                    xmlFree(c);
                }
            }

            if ((p = get_xml_child(n, "new"))) {
                if ((c = xmlNodeGetContent(p))) {
                    if(!*diffInfo->sha256.new && (strlen((const char *)c) == (SHA256_HEX_LENGTH - 1))) {
                        strcpy(diffInfo->sha256.new, (const char *)c);
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

    if (!S(diffInfo->name)) {
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


static pkg_file_t* get_xml_pkg_file(xmlNodePtr ptr) {

    xmlChar * c;
    xmlNodePtr n;

    if (!ptr) { return 0; }

    pkg_file_t * pkgFile = f_malloc(sizeof(pkg_file_t));

    for (n = xmlFirstElementChild(ptr); n; n = xmlNextElementSibling(n)) {

        if (n->type != XML_ELEMENT_NODE) { continue; }

        if (xmlStrEqual(n->name, XMLT "sha256")) {

            if ((c = xmlNodeGetContent(n))) {
                if(!*pkgFile->sha256b64 && (strlen((const char *)c) == (SHA256_B64_LENGTH - 1))) {
                    strcpy(pkgFile->sha256b64, (const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "version")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!pkgFile->version) {
                    pkgFile->version = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "file")) {

            if ((c = xmlNodeGetContent(n))) {
                if (!pkgFile->file) {
                    pkgFile->file = f_strdup((const char *)c);
                }
                xmlFree(c);
            }

        } else if (xmlStrEqual(n->name, XMLT "downloaded")) {

            if ((c = xmlNodeGetContent(n))) {
                pkgFile->downloaded = atoi((const char *)c);
                xmlFree(c);
            }

        }
    }

    if (!S(pkgFile->file) || !S(pkgFile->version) || !S(pkgFile->sha256b64)) {
        DBG("Incomplete pkg node");
        if (pkgFile->file) free(pkgFile->file);
        if (pkgFile->version) free(pkgFile->version);
        free(pkgFile);
        pkgFile = NULL;
    }

    return pkgFile;
}


int parse_pkg_manifest(char * xmlFile, pkg_file_t ** pkgFile) {

    int err = E_UA_OK;
    pkg_file_t * pf;
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
                if ((pf = get_xml_pkg_file(node)) && (!access(pf->file, R_OK))) {
                    DL_APPEND(*pkgFile, pf);
                } else {
                    free(pf);
                    //TODO: Remove from pkg manifest
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


int add_pkg_file_manifest(char * xmlFile, pkg_file_t * pkgFile) {

    int err = E_UA_OK;
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL, node = NULL;
    pkg_file_t * aux;

    do {

        if (!access(xmlFile, W_OK)) {
            BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
            root = xmlDocGetRootElement(doc);
        } else {
            doc = xmlNewDoc("1.0");
            root = xmlNewNode(NULL, XMLT "manifest_pkg");
            xmlDocSetRootElement(doc, root);
        }

        if ((aux = get_xml_version_pkg_file(root, pkgFile->version))) {
            free(aux); break;
            //TODO: replace the entry
        }

        DBG("Adding pkg entry for version: %s in %s", pkgFile->version, xmlFile);

        node = xmlNewChild(root, NULL, XMLT "package", NULL);
        xmlNewChild(node, NULL, XMLT "version", XMLT pkgFile->version);
        xmlNewChild(node, NULL, XMLT "sha256", XMLT pkgFile->sha256b64);
        xmlNewChild(node, NULL, XMLT "file", XMLT pkgFile->file);
        xmlNewChild(node, NULL, XMLT "downloaded", XMLT (pkgFile->downloaded? "1":"0"));


        BOLT_IF((xmlSaveFormatFileEnc(xmlFile, doc, "UTF-8", 1) < 0), E_UA_ERR, "failed to save pkg manifest");

    } while (0);

    if (doc) {
        xmlFreeDoc(doc);
        xmlCleanupParser();
    }

    return err;
}


static pkg_file_t* get_xml_version_pkg_file(xmlNodePtr root, char * version) {

    xmlChar * c;
    xmlNodePtr node, n;
    pkg_file_t * pkgFile = NULL;

    for (node = xmlFirstElementChild(root); node; node = xmlNextElementSibling(node)) {

        if ((node->type == XML_ELEMENT_NODE) && xmlStrEqual(node->name, XMLT "package")) {
            if ((n = get_xml_child(node, XMLT "version"))) {
                if ((c = xmlNodeGetContent(n))) {
                    if (xmlStrEqual(c, XMLT version)) {
                        pkgFile = get_xml_pkg_file(node);
                    }
                    xmlFree(c);
                }
            }
        }

        if (pkgFile) { break; }
    }

    return pkgFile;
}


int get_pkg_file_manifest(char * xmlFile, char * version, pkg_file_t * pkgFile) {

    int err = E_UA_OK;
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL;
    pkg_file_t * pf = NULL;

    if (!xmlFile || !version || !pkgFile) return E_UA_ERR;

    do {
        BOLT_SYS(access(xmlFile, R_OK), "pkg manifest not available %s", xmlFile);
        BOLT_IF(!(doc = xmlReadFile(xmlFile, NULL, 0)), E_UA_ERR, "Could not read xml file %s", xmlFile);

        root = xmlDocGetRootElement(doc);
        BOLT_IF(!(pf = get_xml_version_pkg_file(root, version)), E_UA_ERR, "version %s not found in pkg_manifest %s", version, xmlFile);

        memcpy(pkgFile, pf, sizeof(pkg_file_t));
        free(pf);

    } while (0);

    return err;
}
