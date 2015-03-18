#ifndef OSS_XML_H
#define OSS_XML_H

#include "http.h"

#include <libxml/tree.h>

typedef oss_error_t (*parse_action_t) (char *value, void *user_data);

struct parse_key_action {
    const char *key;
    parse_action_t action;
    void *user_data;
};

oss_error_t parse_kvlist(xmlNode *parent, struct parse_key_action *key_action_list);

void free_xml(void *xml);
oss_error_t xml_get_doc_root(const char *buffer, int size, const char *root_name,
                             xmlDoc **pdoc, xmlNode **proot);

char *create_website_xml(const char *index_file, const char *error_file, int *size);

oss_error_t parse_get_service_response(const char *buffer, int size, struct oss_service *result);

oss_error_t parse_init_upload_response(const char *buffer, int size, char **upload_id);

#endif
