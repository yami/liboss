#include <stdio.h>

#include "object.h"
#include "bucket.h"

#ifdef _WIN32

#define strdup _strdup
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp

#else
#include <strings.h>
#endif


int main(int argc, char **argv)
{
    struct ohttp_connection *conn = NULL;
    oss_error_t status = OSSE_OK;

    ohttp_init();

    conn = ohttp_connection_create("oss-cn-hangzhou", "aliyuncs.com", 80);

    if (!conn) {
        printf("Failed to init connection\n");
        return 1;
    }

    if (argc < 3) {
        printf("invalid number of arguments");
        return 1;
    }

    conn->cred.id = strdup(argv[1]);
    conn->cred.key = strdup(argv[2]);

#if 0
    struct oss_acl acl;
    oss_get_bucket_acl(conn, "ming-oss-share", &acl);

    oss_head_object(conn, "ming-oss-share", "test");
#endif

#if 0
    if (argc < 3) {
        printf("Invalid number of args\n");
        return 2;
    }

    status = oss_get_object_to_file(conn, "ming-oss-share", argv[1], argv[2]);

    if (status != OSSE_OK) {
        printf("Failed to get object: %d", status);
        return 3;
    }
#endif

#if 0
    {
        int i;
        struct oss_service service = OSS_SERVICE_INIT;

        status = oss_get_service(conn, &service);

        if (status != OSSE_OK) {
            printf("Failed get service: %d\n", status);
        } else {
            printf("id:%s display_name:%s\n", service.owner.id, service.owner.display_name);
            printf("bucket list:\n");
            printf("  %16s %16s %16s\n", "Location", "Name", "CreationDate");
            for (i = 0; i < service.nr_buckets; i++) {
                struct oss_bucket *b = &service.buckets[i];
                printf("  %16s %16s %16s\n", b->location, b->name, b->creation_date);
            }
        }
    }
#endif

#if 0
    {
        status = oss_put_bucket_website(conn, "ming-oss-share", "index.html", "error.html");
        if (status != OSSE_OK) {
            printf("put website failed stauts=%d\n", status);
        }
    }
#endif

#if 0
    {
        if (argc < 3) {
            printf("Invalid number of args\n");
            return 2;
        }

        status = oss_put_object_from_file(conn, "ming-oss-share", argv[1], argv[2]);
        
        if (status != OSSE_OK) {
            printf("Failed to put object: %d", status);
            return 3;
        }
    }
#endif

    if (argc < 4) {
        printf("bad number of arguments\n");
        return 1;
    }
    
    status = oss_delete_object(conn, "ming-oss-share", argv[3]);
    if (status != OSSE_OK) {
        printf("failed to delete object: %d\n", status);
        return 2;
    }
    
    return 0;
}
