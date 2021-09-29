/*
 * xml.c
 */

#include "xml.h"
#include "utlist.h"
#include "debug.h"

static xmlNodePtr get_xml_child(xmlNodePtr parent, xmlChar* name);
static diff_info_t* get_xml_diff_info(xmlNodePtr ptr);
static pkg_file_t* get_xml_pkg_file(xmlNodePtr ptr);
static pkg_file_t* get_xml_version_pkg_file(xmlNodePtr root, char* version);

#define XMLELE_ITER(p, c) \
        for (c = xmlFirstElementChild(p); c; c = xmlNextElementSibling(c)) \
		if (c->type != XML_ELEMENT_NODE) continue; else

#define XMLELE_ITER_SAFE(p, c, tmp) \
        for (c = xmlFirstElementChild(p); c && (tmp = xmlNextElementSibling(c), 1); c = tmp) \
		if (c->type != XML_ELEMENT_NODE) continue; else

#define XMLELE_ITER_NAME(p, s, c) \
        XMLELE_ITER(p, c) if (xmlStrEqual(c->name, XMLT s))


static xmlNodePtr get_xml_child(xmlNodePtr parent, xmlChar* name)
{
	xmlNodePtr node = NULL;

	if (!parent) { return 0; }

	XMLELE_ITER_NAME(parent, name, node) {
		return node;
	}

	return 0;
}


static diff_info_t* get_xml_diff_info(xmlNodePtr ptr)
{
	xmlChar* c;
	xmlNodePtr n, p;

	if (!ptr) { return 0; }

	diff_info_t* diffInfo = f_malloc(sizeof(diff_info_t));

	XMLELE_ITER(ptr, n) {
		if (xmlStrEqual(n->name, XMLT "name")) {
			if ((c = xmlNodeGetContent(n))) {
				if (!diffInfo->name) {
					diffInfo->name = f_strdup((const char*)c);
				}
				xmlFree(c);
			}

		} else if (xmlStrEqual(n->name, XMLT "sha256")) {
			if ((p = get_xml_child(n, XMLT "old"))) {
				if ((c = xmlNodeGetContent(p))) {
					if (!*diffInfo->sha256.old && (strlen((const char*)c) == (SHA256_HEX_LENGTH - 1))) {
						strcpy_s(diffInfo->sha256.old, (const char*)c, sizeof(diffInfo->sha256.old));
					}
					xmlFree(c);
				}
			}

			if ((p = get_xml_child(n, XMLT "new"))) {
				if ((c = xmlNodeGetContent(p))) {
					if (!*diffInfo->sha256.new && (strlen((const char*)c) == (SHA256_HEX_LENGTH - 1))) {
						strcpy_s(diffInfo->sha256.new, (const char*)c, sizeof(diffInfo->sha256.new));
					}
					xmlFree(c);
				}
			}

		} else if (xmlStrEqual(n->name, XMLT "format")) {
			if ((c = xmlNodeGetContent(n))) {
				if (!diffInfo->format) {
					diffInfo->format = f_strdup((const char*)c);
				}
				xmlFree(c);
			}

		} else if (xmlStrEqual(n->name, XMLT "compression")) {
			if ((c = xmlNodeGetContent(n))) {
				if (!diffInfo->compression) {
					diffInfo->compression = f_strdup((const char*)c);
				}
				xmlFree(c);
			}
		}
	}

	if (!S(diffInfo->name) ||
	    (xmlStrEqual(ptr->parent->name, XMLT "changed") && (!S(diffInfo->format) || !S(diffInfo->compression)))) {
		A_INFO_MSG("Incomplete diff node");
		free_diff_info(diffInfo);
		diffInfo = NULL;
	}

	return diffInfo;
}

static inline int TYPEQL(const xmlChar* a, const xmlChar* b, diff_type_t* typ)
{
	if (!strcmp((const char*)b,"added"))
		*typ = DT_ADDED;
	else if (!strcmp((const char*)b, "removed"))
		*typ = DT_REMOVED;
	else if (!strcmp((const char*)b, "unchanged"))
		*typ = DT_UNCHANGED;
	else if (!strcmp((const char*)b, "changed"))
		*typ = DT_CHANGED;
	return xmlStrEqual(a, b);
}

int parse_diff_manifest(char* xmlFile, diff_info_t** diffInfo)
{
	int err = E_UA_OK;
	diff_type_t typ;
	diff_info_t* di, * aux, * diList = 0;
	xmlDocPtr doc = NULL;
	xmlNodePtr root, node, fnode = NULL;

	A_INFO_MSG("parsing diff manifest: %s", xmlFile);

	do {
		BOLT_IF(!xmlFile || access(xmlFile, R_OK), E_UA_ERR, "diff manifest not available %s", xmlFile);
		BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
		root = xmlDocGetRootElement(doc);

		XMLELE_ITER(root, node) {
			if (TYPEQL(node->name, XMLT "added", &typ) || TYPEQL(node->name, XMLT "removed", &typ)
			    || TYPEQL(node->name, XMLT "unchanged", &typ) || TYPEQL(node->name, XMLT "changed", &typ)) {
				XMLELE_ITER_NAME(node, "file", fnode) {
					if ((di = get_xml_diff_info(fnode))) {
						di->type = typ;
						DL_APPEND(diList, di);
					} else {
						err = E_UA_ERR;
						break;
					}
				}
			}

			if (err) { break; }
		}

	} while (0);

	if (doc) {
		xmlFreeDoc(doc);
	}

	if (!err) {
		*diffInfo = diList;
	} else {
		DL_FOREACH_SAFE(diList, di, aux) {
			DL_DELETE(diList, di);
			free_diff_info(di);
		}
	}

	return err;
}


static pkg_file_t* get_xml_pkg_file(xmlNodePtr ptr)
{
	xmlChar* c;
	xmlNodePtr n;

	if (!ptr) { return 0; }

	pkg_file_t* pkgFile = f_malloc(sizeof(pkg_file_t));

	XMLELE_ITER(ptr, n) {
		if (xmlStrEqual(n->name, XMLT "sha256")) {
			if ((c = xmlNodeGetContent(n))) {
				if (!*pkgFile->sha256b64 && (strlen((const char*)c) == (SHA256_B64_LENGTH - 1))) {
					strcpy_s(pkgFile->sha256b64, (const char*)c, sizeof(pkgFile->sha256b64));
				}
				xmlFree(c);
			}

		} else if (xmlStrEqual(n->name, XMLT "sha-of-sha")) {
			if ((c = xmlNodeGetContent(n))) {
				if (strlen((const char*)c) == (SHA256_B64_LENGTH - 1)) {
					strcpy_s(pkgFile->sha_of_sha, (const char*)c, sizeof(pkgFile->sha_of_sha));
				}
				xmlFree(c);
			}

		} else if (xmlStrEqual(n->name, XMLT "version")) {
			if ((c = xmlNodeGetContent(n))) {
				if (!pkgFile->version) {
					pkgFile->version = f_strdup((const char*)c);
				}
				xmlFree(c);
			}

		} else if (xmlStrEqual(n->name, XMLT "file")) {
			if ((c = xmlNodeGetContent(n))) {
				if (!pkgFile->file) {
					pkgFile->file = f_strdup((const char*)c);
				}
				xmlFree(c);
			}

		} else if (xmlStrEqual(n->name, XMLT "downloaded")) {
			if ((c = xmlNodeGetContent(n))) {
				pkgFile->downloaded = atoi((const char*)c);
				xmlFree(c);
			}

		}else if (xmlStrEqual(n->name, XMLT "rollback-order")) {
			if ((c = xmlNodeGetContent(n))) {
				pkgFile->rollback_order = atoi((const char*)c);
				xmlFree(c);
			}

		}
	}

	if (!S(pkgFile->file) || !S(pkgFile->version) || !S(pkgFile->sha256b64)) {
		A_INFO_MSG("Incomplete pkg node");
		Z_FREE(pkgFile->file);
		Z_FREE(pkgFile->version);
		Z_FREE(pkgFile);

	}

	return pkgFile;
}


int parse_pkg_manifest(char* xmlFile, pkg_file_t** pkgFile)
{
	int err = E_UA_OK;
	pkg_file_t* pf, * pfList = 0;
	xmlDocPtr doc = NULL;
	xmlNodePtr root, aux, node = NULL;

	A_INFO_MSG("parsing pkg manifest: %s", xmlFile);

	do {
		BOLT_IF(!xmlFile || access(xmlFile, R_OK), E_UA_ERR, "pkg manifest not available %s", xmlFile);
		BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
		root = xmlDocGetRootElement(doc);

		XMLELE_ITER_SAFE(root, node, aux)
		if (xmlStrEqual(node->name, XMLT "package")) {
			if ((pf = get_xml_pkg_file(node))) {
				if (!access(pf->file, R_OK)) {
					DL_PREPEND(pfList, pf);
				} else {
					free_pkg_file(pf);
				}
			}
		}

	} while (0);

	if (doc) {
		xmlFreeDoc(doc);
	}

	if (!err) {
		*pkgFile = pfList;
	}

	return err;
}

int remove_old_backup(char* xmlFile, char* version)
{
	int err       = E_UA_OK;
	xmlDocPtr doc = NULL;
	xmlNodePtr root, node, n, aux;
	xmlChar* c, * backpath;
	char* tmp_dir;

	do {
		A_INFO_MSG("Cleaning up backup after installing version: %s in %s", version, xmlFile);
		if (!access(xmlFile, W_OK)) {
			BOLT_SYS(!(doc = xmlReadFile(xmlFile, NULL, 0)), "Could not read xml file %s", xmlFile);
			root = xmlDocGetRootElement(doc);
		} else {
			doc  = xmlNewDoc(XMLT "1.0");
			root = xmlNewNode(NULL, XMLT "manifest_pkg");
			xmlDocSetRootElement(doc, root);
		}

		XMLELE_ITER_SAFE(root, node, aux)
		if (xmlStrEqual(node->name, XMLT "package")) {
			if ((n = get_xml_child(node, XMLT "version"))) {
				if ((c = xmlNodeGetContent(n))) {
					if (!xmlStrEqual(c, XMLT version)) {
						A_INFO_MSG("Removing pkg entry for version: %s in %s", c, xmlFile);
						if ((n = get_xml_child(node, XMLT "file"))) {
							if ((backpath = xmlNodeGetContent(n))) {
								tmp_dir = f_dirname((const char*)backpath);
								if (tmp_dir) {
									rmdirp(tmp_dir);
									free(tmp_dir);
								}
								xmlUnlinkNode(node);
								xmlFreeNode(node);

								BOLT_SYS(chkdirp(xmlFile), "failed to prepare directory for %s", xmlFile);
								BOLT_IF((xmlSaveFormatFileEnc(xmlFile, doc, "UTF-8", 1) < 0), E_UA_ERR, "failed to save pkg manifest");
							}

							xmlFree(backpath);
						}
					}
					xmlFree(c);
				}
			}
		}

		if (xmlChildElementCount(root) == 0)
			unlink(xmlFile);

		tmp_dir = f_dirname((const char*)xmlFile);
		if (tmp_dir) {
			remove_subdirs_except(tmp_dir, version);
			free(tmp_dir);
		}

	} while (0);

	if (doc) {
		xmlFreeDoc(doc);
	}

	return err;
}

int add_pkg_file_manifest(char* xmlFile, pkg_file_t* pkgFile)
{
	int err       = E_UA_OK;
	xmlDocPtr doc = NULL;
	xmlNodePtr root, node, n, aux;
	xmlChar* c;
	char rb_order[32] = {0};

	do {
		if (!access(xmlFile, W_OK) && (doc = xmlReadFile(xmlFile, NULL, 0))) {
			root = xmlDocGetRootElement(doc);
		} else {
			doc  = xmlNewDoc(XMLT "1.0");
			root = xmlNewNode(NULL, XMLT "manifest_pkg");
			xmlDocSetRootElement(doc, root);
		}

		XMLELE_ITER_SAFE(root, node, aux)
		if (xmlStrEqual(node->name, XMLT "package")) {
			if ((n = get_xml_child(node, XMLT "version"))) {
				if ((c = xmlNodeGetContent(n))) {
					if (xmlStrEqual(c, XMLT pkgFile->version)) {
						A_INFO_MSG("Removing pkg entry for version: %s in %s", pkgFile->version, xmlFile);
						xmlUnlinkNode(node);
						xmlFreeNode(node);
					}
					xmlFree(c);
				}
			}
			else if ((n = get_xml_child(node, XMLT "rollback-order"))) {
				//Increment rollback-order for each version.
				if ((c = xmlNodeGetContent(n))) {
					int rc = (int)snprintf(rb_order, sizeof(rb_order), "%d", atoi((const char*)c)+1);
					if (rc > 0 && rc < (int)sizeof(rb_order))
						xmlNodeSetContent(n, XMLT rb_order);
					else
						err = E_UA_ERR;
				}
				xmlFree(c);
			}
		}

		if (!sha256xcmp(pkgFile->file, pkgFile->sha_of_sha)) {
			A_INFO_MSG("Adding pkg entry for version: %s in %s", pkgFile->version, xmlFile);

			node = xmlNewChild(root, NULL, XMLT "package", NULL);
			xmlNewChild(node, NULL, XMLT "version", XMLT pkgFile->version);
			xmlNewChild(node, NULL, XMLT "sha256", XMLT pkgFile->sha256b64);
			xmlNewChild(node, NULL, XMLT "sha-of-sha", XMLT pkgFile->sha_of_sha);
			xmlNewChild(node, NULL, XMLT "file", XMLT pkgFile->file);
			xmlNewChild(node, NULL, XMLT "downloaded", XMLT (pkgFile->downloaded ? "1" : "0"));
			xmlNewChild(node, NULL, XMLT "rollback-order", XMLT "0");

			BOLT_SYS(chkdirp(xmlFile), "failed to prepare directory for %s", xmlFile);
			BOLT_IF((xmlSaveFormatFileEnc(xmlFile, doc, "UTF-8", 1) < 0), E_UA_ERR, "failed to save pkg manifest");

		} else {
			A_INFO_MSG("Skipping pkg entry for version: %s in %s", pkgFile->version, xmlFile);
			A_INFO_MSG("Possibly corrupted package file (%s), not matched with exptected sha value (%s)", pkgFile->file, pkgFile->sha256b64);
		}

	} while (0);

	if (doc) {
		xmlFreeDoc(doc);
	}

	return err;
}


static pkg_file_t* get_xml_version_pkg_file(xmlNodePtr root, char* version)
{
	xmlChar* c;
	xmlNodePtr node, n;
	pkg_file_t* pkgFile = NULL;

	XMLELE_ITER_NAME(root, "package", node) {
		if ((n = get_xml_child(node, XMLT "version"))) {
			if ((c = xmlNodeGetContent(n))) {
				if (xmlStrEqual(c, XMLT version)) {
					pkgFile = get_xml_pkg_file(node);
				}
				xmlFree(c);
			}
		}

		if (pkgFile) { break; }
	}

	return pkgFile;
}


int get_pkg_file_manifest(char* xmlFile, char* version, pkg_file_t* pkgFile)
{
	int err         = E_UA_OK;
	xmlDocPtr doc   = NULL;
	xmlNodePtr root = NULL;
	pkg_file_t* pf  = NULL;

	if (!xmlFile || !version || !pkgFile) {
		A_ERROR_MSG("Got null pointer(s) xmlFile(%p), version(%p), pkgFile(%p)", xmlFile, version, pkgFile);
		return E_UA_ERR;
	}

	do {
		BOLT_SYS(access(xmlFile, R_OK), "pkg manifest (%s)", xmlFile);
		BOLT_IF(!(doc = xmlReadFile(xmlFile, NULL, 0)), E_UA_ERR, "Could not read xml file %s", xmlFile);

		root = xmlDocGetRootElement(doc);
		BOLT_IF(!(pf = get_xml_version_pkg_file(root, version)), E_UA_ERR, "version %s not found in pkg_manifest %s", version, xmlFile);

		memcpy(pkgFile, pf, sizeof(pkg_file_t));
		free(pf);

	} while (0);

	if (doc) {
		xmlFreeDoc(doc);
	}
	return err;
}

void free_pkg_file(pkg_file_t* pkgFile)
{
	Z_FREE(pkgFile->version);
	Z_FREE(pkgFile->file);
	Z_FREE(pkgFile);

}