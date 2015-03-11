#ifndef OSS_XML_H
#define OSS_XML_H

#include "http.h"

void free_xml(void *xml);

char *create_website_xml(const char *index_file, const char *error_file, int *size);

oss_error_t parse_get_service_response(const char *buffer, int size, struct oss_service *result);

oss_error_t parse_init_upload_response(const char *buffer, int size, char **upload_id);

#endif
