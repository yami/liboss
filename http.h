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


struct ohttp_connection {
    /* public */
    char *region;
    char *host;
    int port;

    struct oss_credential cred;

    /* public */
    struct ohttp_request request;
    struct ohttp_response response;

    struct ohttp_io *io;

    /* private */
    CURL* curl;
    struct curl_slist *curl_headers;
};

enum ohttp_method {
    OMETHOD_UNKNOWN,            /* must be the first one */
    OMETHOD_DELETE,
    OMETHOD_GET,
    OMETHOD_HEAD,
    OMETHOD_POST,
    OMETHOD_PUT,
};

typedef oss_i64 (* ohttp_io_on_send_body_t) (char *p, size_t len, void *self);
typedef oss_i64 (* ohttp_io_on_recv_body_t) (char *p, size_t len, void *self);

typedef oss_error_t (* ohttp_io_end_recv_body_t) (void *self);

typedef void (* ohttp_io_free_t) (void *self);

struct ohttp_io {
    ohttp_io_on_send_body_t on_send_body;
    ohttp_io_on_recv_body_t  on_recv_body;
    
    ohttp_io_end_recv_body_t end_recv_body;

    ohttp_io_free_t free;
};

void ohttp_io_free(struct ohttp_io *io);

struct ohttp_memio {
    struct ohttp_io super;

    int send_offset;
    struct oss_buffer send_buffer;
    
    struct oss_buffer recv_buffer;
};

struct ohttp_memio *ohttp_memio_send_create(const char *ptr, int size);
struct ohttp_memio *ohttp_memio_recv_create();

struct ohttp_fio {
    struct ohttp_io super;

    FILE *fh;
};

oss_i64 ohttp_fio_on_recv_body(char *p, size_t len, void *self);
oss_i64 ohttp_fio_on_send_body(char *p, size_t len, void *self);

struct ohttp_fio *ohttp_fread_create(FILE *fh);
struct ohttp_fio *ohttp_fwrite_create(FILE *fh);


oss_error_t ohttp_init();

struct ohttp_connection * ohttp_connection_create(const char *region, const char *host, int port);
void ohttp_connection_free_all(struct ohttp_connection *conn);

void ohttp_connection_clear(struct ohttp_connection *conn);

oss_error_t ohttp_make_url(struct ohttp_connection *conn, const char *bucket, const char *object);
oss_error_t ohttp_make_auth_header(struct ohttp_request *request, struct oss_credential *cred, const char *bucket, const char *object);

void ohttp_set_io(struct ohttp_connection *conn, struct ohttp_io *io);
oss_error_t ohttp_set_credential(struct ohttp_connection *conn, const char *access_id, const char *access_key);

oss_error_t ohttp_request(struct ohttp_connection *conn, 
                          enum ohttp_method method,
                          const char *bucket,
                          const char *object);
#endif
