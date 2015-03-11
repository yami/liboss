#include "xml.h"
#include "bucket.h"
#include "logging.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <assert.h>

void free_xml(void *xml)
{
    xmlFree(xml);
}

xmlNode *xml_new_root(xmlDoc *doc, const char *name)
{
    xmlNode *root = xmlNewNode(NULL, BAD_CAST name);
    if (!root) {
        ologe("failed to new root: %s", name);
        return NULL;
    }

    xmlDocSetRootElement(doc, root);

    return root;
}

xmlNode *xml_new_child(xmlNode *parent, const char *name)
{
    xmlNode *child = xmlNewChild(parent, NULL, BAD_CAST name, NULL);
    if (!child) {
        ologe("failed to new node: %s", name);
    }

    return child;
}

xmlNode *xml_new_text_child(xmlNode *parent, const char *name, const char *text)
{
    xmlNode *child = xmlNewTextChild(parent, NULL, BAD_CAST name, BAD_CAST text);
    if (!child) {
        ologe("failed to new text node: %s", name);
    }

    return child;
}

char *create_website_xml(const char *index_file, const char *error_file, int *size)
{
    xmlChar *xml = NULL;
    
    xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNode *root;
    xmlNode *index_doc;
    xmlNode *error_doc;
    xmlNode *index_suffix;
    xmlNode *error_key;

    if (!doc) {
        ologe("failed to new doc");
        return NULL;
    }

    if (!(root = xml_new_root(doc, "WebsiteConfiguration")))
        goto error;    

    if (!(index_doc = xml_new_child(root, "IndexDocument")))
        goto error;

    if (!(index_suffix = xml_new_text_child(index_doc, "Suffix", index_file)))
        goto error;

    if (!(error_doc = xml_new_child(root, "ErrorDocument")))
        goto error;

    if (!(error_key = xml_new_text_child(error_doc, "Key", error_file)))
        goto error;

    xmlDocDumpMemory(doc, &xml, size);
    
error:
    xmlFreeDoc(doc);
    return (char *) xml;
}

static
oss_error_t xml_get_doc_root(const char *buffer, int size, const char *root_name,
                             xmlDoc **pdoc, xmlNode **proot)
{
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    
    *pdoc = NULL;
    *proot = NULL;

    doc = xmlReadMemory(buffer, size, NULL, NULL, 0);
    if (!doc) {
        ologe("failed to load xml from memory, root_name=%s", root_name);
        return OSSE_XML;
    }

    root = xmlDocGetRootElement(doc);
    if (!root) {
        ologe("failed to get root node, root_name=%s", root_name);
        
        xmlFreeDoc(doc);
        return OSSE_XML;
    }

    if (strcmp((const char *) root->name, root_name) != 0) {
        ologe("root mismatch, xml_root=%s root_name=%s", root->name, root_name);

        xmlFreeDoc(doc);
        return OSSE_XML;
    }

    *pdoc = doc;
    *proot = root;

    return OSSE_OK;
}

static
char *get_node_text(xmlNode *node)
{
    char *text = NULL;
    xmlChar *xml_text = xmlNodeGetContent(node);

    if (!xml_text)
        return text;

    text = strdup((const char *)xml_text);

    xmlFree(xml_text);

    return text;
}

static
int count_child_elements(xmlNode *buckets_node, const char *name)
{
    xmlNode *node;
    int n = 0;
    
    for (node = buckets_node->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        if (!strcmp((const char *)node->name, name))
            ++n;
    }

    return n;
}

typedef oss_error_t (*parse_action_t) (char *value, void *user_data);

struct parse_key_action {
    const char *key;
    parse_action_t action;
    void *user_data;
};

#define MAX_NR_KEY_ACTION 1024

static
oss_error_t action_get_string(char *value, void *user_data)
{
    char **p = (char **) user_data;

    free(*p);
    *p = value;

    return OSSE_OK;
}

oss_error_t parse_kvlist(xmlNode *parent, struct parse_key_action *key_action_list)
{
    struct parse_key_action *key_action;
    xmlNode *node;

    for (node = parent->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        for (key_action = key_action_list; key_action->key; ++key_action) {
            assert(key_action - key_action_list < MAX_NR_KEY_ACTION);

            if (!strcmp((const char *) node->name, key_action->key)) {
                enum oss_error status;
                char *value;
                
                if (!(value = get_node_text(node)))
                    return OSSE_NO_MEMORY;
                
                if ((status = key_action->action(value, key_action->user_data)) != OSSE_OK)
                    return status;
                
                break;
            }
        }
    }

    return OSSE_OK;
}

oss_error_t parse_get_service_buckets(xmlNode *buckets_node, struct oss_bucket **out_buckets, int *out_nr_buckets)
{
    xmlNode *node = NULL;

    struct oss_bucket *buckets;
    int ibucket = 0;
    int nr_buckets = count_child_elements(buckets_node, "Bucket");

    if (!nr_buckets)
        return OSSE_OK;

    if (!(buckets = (struct oss_bucket *) calloc(nr_buckets, sizeof(*buckets))))
        goto error;
    
    for (node = buckets_node->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        if (!strcmp((const char *)node->name, "Bucket")) {
            struct oss_bucket *b = &buckets[ibucket];
            
            struct parse_key_action actions[] = {
                {"Location",     action_get_string, &b->location},
                {"Name",         action_get_string, &b->name},
                {"CreationDate", action_get_string, &b->creation_date},
                {NULL}
            };
            
            assert(ibucket < nr_buckets);

            if (parse_kvlist(node, actions) != OSSE_OK)
                goto error;

            ++ibucket;
        }
    }

    *out_buckets = buckets;
    *out_nr_buckets = nr_buckets;
    
    return OSSE_OK;
    
error:
    free(buckets);
    
    return OSSE_XML;
}

oss_error_t parse_get_service_response(const char *buffer, int size, struct oss_service *service)
{
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    xmlNode *node = NULL;

    oss_error_t status = xml_get_doc_root(buffer, size, "ListAllMyBucketsResult", &doc, &root);

    if (status != OSSE_OK)
        return status;

    for (node = root->children; node; node = node->next) {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        if (!strcmp((const char *) node->name, "Owner")) {
            struct parse_key_action actions[] = {
                {"ID",          action_get_string, &service->owner.id},
                {"DisplayName", action_get_string, &service->owner.display_name},
                {NULL}
            };
            
            if (parse_kvlist(node, actions) != OSSE_OK)
                goto error;
        }
        else if (!strcmp((const char *) node->name, "Buckets")) {
            if (parse_get_service_buckets(node, &service->buckets, &service->nr_buckets) != OSSE_OK) {
                goto error;
            }
        }
    }

    status = OSSE_OK;

error:
    xmlFreeDoc(doc);
    
    return status;
}

oss_error_t parse_init_upload_response(const char *buffer, int size, char **upload_id)
{
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;
    
    oss_error_t status = xml_get_doc_root(buffer, size, "InitiateMultipartUploadResult", &doc, &root);

    if (status != OSSE_OK)
        return status;

    {
        struct parse_key_action actions[] = {
            {"UploadId", action_get_string, upload_id},
            {NULL}
        };

        if ((status = parse_kvlist(root, actions)) != OSSE_OK)
            return status;
    }

    return status;
}
