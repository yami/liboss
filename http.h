#ifndef OSS_HTTP_H
#define OSS_HTTP_H

#include "util.h"
#include "error_code.h"

#include <curl/curl.h>

struct oss_credential {
    char *id;
    char *key;
};

struct ohttp_request {
    enum ohttp_method method;

    struct oss_kvlist queries;
    struct oss_kvlist headers;

    char *url;
};

struct ohttp_response {
    int status;

    char *reqid;                /* request id */
    
    struct oss_kvlist headers;
    struct oss_buffer ebody;    /* error body */
    struct oss_kvlist edetails; /* error details */
};


typedef oss_error_t (* ohttp_end_recv_body_t) (void *user_data);
typedef oss_i64 (* ohttp_on_recv_body_t) (char *p, size_t len, void *user_data);
typedef oss_i64 (* ohttp_on_send_body_t) (char *p, size_t len, void *user_data);

struct ohttp_connection {
    /* public */
    char *region;
    char *host;
    int port;

    struct oss_credential cred;

    /* public - generated */
    char *end_point;

    /* public */
    struct ohttp_request request;
    struct ohttp_response response;

    struct oss_io *io;

    /* private */
    CURL* curl;
    struct curl_slist *curl_headers;
};

enum ohttp_method {
    OMETHOD_DELETE,
    OMETHOD_GET,
    OMETHOD_HEAD,
    OMETHOD_PUT,
};

struct oss_io {
    ohttp_on_recv_body_t on_recv_body;
    ohttp_end_recv_body_t end_recv_bdoy;
    
    ohttp_on_send_body_t on_send_body;
};

struct oss_memio {
    struct oss_io super;

    int send_offset;
    struct oss_buffer send_buffer;
    
    struct oss_buffer recv_buffer;
};

struct oss_io *oss_memin_create(const char *ptr, int size);
struct oss_io *oss_memout_create();

oss_error_t ohttp_init();

struct ohttp_connection * ohttp_connection_create(const char *region, const char *host, int port);
void ohttp_connection_free(struct ohttp_connection *conn);

oss_error_t ohttp_make_url(struct ohttp_connection *conn, const char *bucket, const char *object);
oss_error_t ohttp_make_auth_header(struct ohttp_request *request, struct oss_credential *cred, const char *bucket, const char *object);

oss_error_t ohttp_request(struct ohttp_connection *conn, 
                             enum ohttp_method method,
                             const char *bucket,
                             const char *object,
                             struct oss_io *io);
#endif
