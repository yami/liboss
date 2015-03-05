#ifndef OSS_BUCKET_H
#define OSS_BUCKET_H

#include "http.h"

struct oss_owner {
    char *id;
    char *display_name;
};

struct oss_bucket {
    char *location;
    char *name;
    char *creation_date;
};

struct oss_service {
    struct oss_owner owner;

    struct oss_bucket *buckets;
    int nr_buckets;
};

#define OSS_SERVICE_INIT {{0}}

enum oss_bucket_acl_mode {
    OACL_PRIVATE,
    OACL_PUBLIC_RD,
    OACL_PUBLIC_RW,
};

struct oss_bucket_acl {
    char *owner_id;
    char *owner_name;

    enum oss_bucket_acl_mode mode;
};


enum oss_error oss_get_service(struct ohttp_connection *conn, struct oss_service *service);

enum oss_error oss_get_bucket_acl(struct ohttp_connection *conn, const char *bucket, struct oss_bucket_acl *acl);
enum oss_error oss_put_bucket_website(struct ohttp_connection *conn, const char *bucket, const char *index_file, const char *error_file);

#endif
