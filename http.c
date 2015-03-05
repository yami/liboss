#include "http.h"

#include "base64.h"
#include "asprintf.h"
#include "logging.h"

#include <stdio.h>
#include <openssl/hmac.h>

const char* s_user_agent = "test";

#define ohttp_conn_io_has(CONN, HANDLE) ((CONN)->io && (CONN)->io->HANDLE)

#define x_easy_setopt(opt, val)                         \
    if ((curl_status = curl_easy_setopt(                \
             conn->curl, opt, val)) != CURLE_OK) {      \
        return OSS_CURLE(curl_status);                  \
    }


static
void ohttp_header_to_kv(const char *buffer, size_t buflen, char **key, char **value)
{
    *key = NULL;
    *value = NULL;

    /* TODO: MING: implement it */
    *key = strdup("key");
    *value = strdup("value");
}

static
size_t ohttp_curl_header_function(void* ptr, size_t size, size_t nmemb, void *ctx)
{
    struct ohttp_connection *conn = (struct ohttp_connection *) ctx;

    char* buffer = ptr;
    size_t buflen = size * nmemb;

    struct ohttp_response *response = &conn->response;

    char* key;
    char* value;

    ohttp_header_to_kv(buffer, buflen, &key, &value);
    
    oss_kvlist_append_nodup(&response->headers, key, value);

    return buflen;
}

static
size_t ohttp_response_error_append(struct ohttp_response *response, char *ptr, size_t len)
{
    if (oss_buffer_append(&response->ebody, ptr, len)) {
        return len;
    } else {
        ologe("ohttp_response_error_append failed\n");
        return 0;
    }
}

static
size_t ohttp_curl_read_function(char *ptr, size_t size, size_t nmemb, void *ctx)
{
    size_t len = size * nmemb;
    struct ohttp_connection *conn = (struct ohttp_connection *) ctx;

    if (ohttp_conn_io_has(conn, on_send_body)) {
        oss_i64 n = conn->io->on_send_body(ptr, len, conn->io);
        if (n < 0)
            return CURL_READFUNC_ABORT;
        else
            return n;
    }

    return CURL_READFUNC_ABORT;
}

static
void set_response_status(struct ohttp_connection *conn)
{
    struct ohttp_response *response = &conn->response;
    
    if (response->status == OSSE_UNKNOWN) {
        CURLcode curl_status;
        long http_status;
        
        if ((curl_status = curl_easy_getinfo(conn->curl, CURLINFO_RESPONSE_CODE, &http_status)) != CURLE_OK) {
            response->status = OSS_CURLE(curl_status);
        } else {
            response->status = http_status;
        }

        ologd("info status is %d\n", response->status);
    }
}

static
size_t ohttp_curl_write_function(char *ptr, size_t size, size_t nmemb, void *ctx)
{
    size_t len = size * nmemb;

    struct ohttp_connection *conn = (struct ohttp_connection *) ctx;
    struct ohttp_response *response = &conn->response;

    set_response_status(conn);

    if (response->status < 0) {
        ologe("error status is %d\n", response->status);
        return 0;
    }

    if (response->status >= 400) {
        return ohttp_response_error_append(response, ptr, len);
    }

    if (response->status >= 200 && response->status < 300) {
        if (ohttp_conn_io_has(conn, on_recv_body))
            return conn->io->on_recv_body(ptr, len, conn->io);
    }

    return len;
}

static
oss_error_t ohttp_curl_set_headers(struct ohttp_connection* conn)
{
    struct oss_kvlist* headers = &conn->request.headers;
    int i;

    for (i = 0; i < headers->n; ++i) {
        char *header_line = NULL;
        struct curl_slist * new_list = NULL;
        
        if (asprintf(&header_line, "%s: %s", headers->items[i].key, headers->items[i].value) == -1)
            return OSSE_NO_MEMORY;

        ologd("add header = %s", header_line);

        new_list = curl_slist_append(conn->curl_headers, header_line);

        free(header_line);

        if (!new_list)
            return OSSE_NO_MEMORY;

        conn->curl_headers = new_list;
    }

    if (curl_easy_setopt(conn->curl, CURLOPT_HTTPHEADER, conn->curl_headers) != CURLE_OK)
        return OSSE_NO_MEMORY;

    return OSSE_OK;
}

oss_error_t ohttp_request_init(struct ohttp_request *request)
{
    request->url = NULL;

    if (!oss_kvlist_init(&request->headers, 0))
        return OSSE_NO_MEMORY;
    
    if (!oss_kvlist_init(&request->queries, 0))
        return OSSE_NO_MEMORY;

    return OSSE_OK;
}

oss_error_t ohttp_response_init(struct ohttp_response *response)
{
    response->status = OSSE_UNKNOWN;

    if (!oss_kvlist_init(&response->headers, 0))
        return OSSE_NO_MEMORY;

    if (!oss_buffer_init(&response->ebody, 0))
        return OSSE_NO_MEMORY;

    return OSSE_OK;
}


/* TODO: MING: rename user_data to something meaningful and related to io */
oss_i64 oss_memio_on_send_body(char *p, size_t len, void *user_data)
{
    struct oss_memio *io = (struct oss_memio *) user_data;
    int nr_send = -1;
    int nr_total = io->send_buffer.n;

    ologd("len=%d send_offset=%d nr_total=%d", (int)len, io->send_offset, nr_total);
    
    if (io->send_offset > nr_total)
        return nr_send;

    if (io->send_offset == nr_total)
        return 0;

    if (io->send_offset + len > nr_total)
        nr_send = nr_total - io->send_offset;
    else
        nr_send = len;

    memcpy(p, io->send_buffer.data + io->send_offset, nr_send);
    io->send_offset += nr_send;

    return nr_send;
}

oss_i64 oss_memio_on_recv_body(char *p, size_t len, void *user_data)
{
    struct oss_memio *io = (struct oss_memio *) user_data;
    
    if (oss_buffer_append(&io->recv_buffer, p, len))
        return len;
    else
        return 0;
}

oss_error_t oss_memio_end_recv_body(void *user_data)
{
    struct oss_memio *io = (struct oss_memio *) user_data;
    if (oss_buffer_append_zero(&io->recv_buffer))
        return OSSE_OK;
    else
        return OSSE_NO_MEMORY;
}

/* the naming is wrong? */
struct oss_io *oss_memin_create(const char *ptr, int size)
{
    struct oss_memio *io = calloc(1, sizeof(*io));
    if (!io)
        return NULL;

    if (!oss_buffer_assign(&io->send_buffer, ptr, size)) {
        free(io);
        return NULL;
    }
    
    io->super.on_send_body = &oss_memio_on_send_body;

    return (struct oss_io *) io;
}

struct oss_io *oss_memout_create()
{
    struct oss_memio *io = calloc(1, sizeof(*io));
    if (!io)
        return NULL;

    io->super.on_recv_body = &oss_memio_on_recv_body;
    io->super.end_recv_bdoy = &oss_memio_end_recv_body;

    return (struct oss_io *) io;
}

struct ohttp_connection * ohttp_connection_create(const char *region, const char *host, int port)
{
    struct ohttp_connection *conn = calloc(1, sizeof(*conn));

    if (!conn)
        return NULL;

    conn->region = strdup(region);
    conn->host = strdup(host);
    conn->port = port;

    conn->curl = curl_easy_init();
    
    if (ohttp_request_init(&conn->request) != OSSE_OK)
        goto error;

    if (ohttp_response_init(&conn->response) != OSSE_OK)
        goto error;

    return conn;

error:
    ohttp_connection_free(conn);
    return NULL;
}

void ohttp_connection_free(struct ohttp_connection *conn)
{
    curl_easy_cleanup(conn->curl);
}

oss_error_t ohttp_init()
{
    CURLcode code = curl_global_init(CURL_GLOBAL_ALL);

    return (code == CURLE_OK) ? OSSE_OK : OSS_CURLE(code);
}

const char * ohttp_method_string(enum ohttp_method method)
{
    switch (method) {
        case OMETHOD_DELETE:
            return "DELETE";
        case OMETHOD_GET:
            return "GET";
        case OMETHOD_HEAD:
            return "HEAD";
        case OMETHOD_PUT:
            return "PUT";
    }

    return "UNKNOWN";
}

const char * ohttp_request_get_header(struct ohttp_request *request, const char *key)
{
    struct oss_kv *header = oss_kvlist_ifind(&request->headers, key);

    return header ? header->value : "";
}


static
char * sign_buffer_appendln(struct oss_buffer *buffer, const char *str)
{
    char *back;
    char *ret = oss_buffer_append(buffer, str, strlen(str) + 1);
    
    if (!ret)
        return ret;

    back = oss_buffer_back(buffer);
    *back = '\n';

    return ret;
}

static
char * sign_buffer_append(struct oss_buffer *buffer, const char *str)
{
    return oss_buffer_append(buffer, str, strlen(str));
}


/* TODO: MING: optimize this function */
static
char * sign_buffer_append_header(struct oss_buffer *buffer, struct oss_kv *kv)
{
    char *norm_key = str_duplower(kv->key);
    char *kv_string = NULL;
    char *ret = NULL;

    if (!norm_key)
        goto error;

    if (asprintf(&kv_string, "%s:%s", norm_key, kv->value) == -1)
        goto error;

    ret = sign_buffer_appendln(buffer, kv_string);

error:
    free(norm_key);
    free(kv_string);

    return ret;
}

static
char * sign_buffer_append_query(struct oss_buffer *buffer, struct oss_kv *kv, int *first_query)
{
    if (*first_query) {
        if (!sign_buffer_append(buffer, "?"))
            return NULL;

        *first_query = 0;
    } else {
        if (!sign_buffer_append(buffer, "&"))
            return NULL;
    }

    if (kv->value[0]) {
        if (!sign_buffer_append(buffer, kv->key))
            return NULL;

        if (!sign_buffer_append(buffer, "="))
            return NULL;

        return sign_buffer_append(buffer, kv->value);
    } else {
        return sign_buffer_append(buffer, kv->key);
    }
}

static const char *s_subresources[] = {
    "uploadId", "uploads", "partNumber",
    "acl", "delete", "website", "location", "qos", "referer", "logging", "cors", "lifecycle",
    "response-content-type", 
    "response-content-language",
    "response-cache-control", 
    "response-content-encoding",
    "response-expires", 
    "response-content-disposition", 
    NULL
};

int ohttp_is_subresource(const char *str)
{
    const char **sub;
    for (sub = &s_subresources[0]; *sub; sub++) {
        if (strcmp(str, *sub) == 0)
            return 1;
    }

    return 0;
}


/* TODO: MING: sort headers, subresources before this function */
/* TODO: MING: make it better */
char * ohttp_get_string_to_sign(struct ohttp_request *request, const char *bucket, const char *object)
{
    char *string_to_sign = NULL;

    struct oss_kvlist *headers = &request->headers;
    struct oss_kvlist *queries = &request->queries;

    int first_query = 1;

    int i;
    
    struct oss_buffer s;
    oss_buffer_init(&s, 1024);

    if (!sign_buffer_appendln(&s, ohttp_method_string(request->method)))
        goto error;

    if (!sign_buffer_appendln(&s, ohttp_request_get_header(request, "content-md5")))
        goto error;

    if (!sign_buffer_appendln(&s, ohttp_request_get_header(request, "content-type")))
        goto error;

    if (!sign_buffer_appendln(&s, ohttp_request_get_header(request, "date")))
        goto error;

    for (i = 0; i < headers->n; i++) {
        if (istarts_with(headers->items[i].key, "x-oss-")) {
            if (!sign_buffer_append_header(&s, &headers->items[i]))
                goto error;
        }
    }

    if (!sign_buffer_append(&s, "/"))
        goto error;

    if (bucket) {
        if (!sign_buffer_append(&s, bucket))
            goto error;

        if (!sign_buffer_append(&s, "/"))
            goto error;

        if (object && !sign_buffer_append(&s, object))
            goto error;
    }

    for (i = 0; i < queries->n; i++) {
        if (ohttp_is_subresource(queries->items[i].key)) {
            ologd("ohttp_get_string_to_sign subresource:%s", queries->items[i].key);
            if (!sign_buffer_append_query(&s, &queries->items[i], &first_query))
                goto error;
        }
    }

    string_to_sign = oss_buffer_detach_as_string(&s);
    
    ologd("ohttp_get_string_to_sign string_to_sign:%s", string_to_sign);

error:
    oss_buffer_free(&s);
    return string_to_sign;
}

char *calculate_signature(struct oss_credential *cred, const char *string_to_sign)
{
    char *binary_sign = malloc(EVP_MAX_MD_SIZE);
    unsigned int binary_len;

    int b64_strlen;
    char *b64_sign = NULL;
    
    if (!binary_sign)
        return NULL;

    HMAC(EVP_sha1(), cred->key, strlen(cred->key), string_to_sign, strlen(string_to_sign), binary_sign, &binary_len);
    
    b64_sign = base64(binary_sign, binary_len, &b64_strlen);

    free(binary_sign);

    return b64_sign;
}

char * ohttp_request_make_query_string(struct ohttp_request *request)
{
    struct oss_kvlist *queries = &request->queries;
    char *query_string = NULL;
    struct oss_buffer b;

    int i;
    
    if (!queries->n)
        return strdup("");
    
    if (!oss_buffer_init(&b, 16))
        return NULL;

    if (!oss_buffer_append(&b, "?", 1))
        goto error;
    
    for (i = 0; i < queries->n; i++) {
        struct oss_kv *kv = &queries->items[i];
        
        if (kv->value[0]) {
            if (!oss_buffer_append(&b, kv->key, strlen(kv->key)))
                goto error;

            if (!oss_buffer_append(&b, "=", 1))
                goto error;

            if (!oss_buffer_append(&b, kv->value, strlen(kv->value)))
                goto error;
        } else {
            if (!oss_buffer_append(&b, kv->key, strlen(kv->key)))
                goto error;
        }
    }

    return oss_buffer_detach_as_string(&b);

error:
    oss_buffer_free(&b);
    return NULL;
}

/* TODO: MING: unify convention for naming 'make', 'gen' etc. For example, make may mean make internal member. */
oss_error_t ohttp_make_url(struct ohttp_connection *conn, const char *bucket, const char *object)
{
    oss_error_t status = OSSE_NO_MEMORY;

    struct ohttp_request *request = &conn->request;
    
    char *encoded_object = NULL;
    char *query = NULL;
    
    if (request->queries.n) {
        if (!(query = ohttp_request_make_query_string(request)))
            goto error;
    }

    
    if (object) {
        if (!(encoded_object = url_encode(object)))
            goto error;

        if (asprintf(&request->url, "http://%s.%s.%s:%d/%s%s",
                     bucket, conn->region, conn->host, conn->port, object,
                     query ? query : "") == -1)
            goto error;
    } else if (bucket) {
        if (asprintf(&request->url, "http://%s.%s.%s:%d/%s",
                     bucket, conn->region, conn->host, conn->port,
                     query ? query : "") == -1) {
            goto error;
        }
    } else {
        if (asprintf(&request->url, "http://%s.%s:%d/%s",
                     conn->region, conn->host, conn->port,
                     query ? query : "") == -1)
            goto error;
    }

    status = OSSE_OK;
    
error:
    free(encoded_object);
    free(query);
    
    return status;
}

oss_error_t ohttp_make_auth_header(struct ohttp_request *request, struct oss_credential *cred, const char *bucket, const char *object)
{
    char *string_to_sign = NULL;
    char *signature = NULL;
    char *auth_value = NULL;

    oss_error_t status = OSSE_NO_MEMORY;

    if (!(string_to_sign = ohttp_get_string_to_sign(request, bucket, object)))
        goto error;

    if (!(signature = calculate_signature(cred, string_to_sign)))
        goto error;

    if (asprintf(&auth_value, "OSS %s:%s", cred->id, signature) == -1)
        goto error;

    ologd("string_to_sign = %s", string_to_sign);
    ologd("auth_value = %s", auth_value);

    if (!oss_kvlist_append(&request->headers, "Authorization", auth_value))
        goto error;

    status = OSSE_OK;

error:
    free(string_to_sign);
    free(signature);
    free(auth_value);

    return status;
}

/* TODO: MING: the end of this func is not confirmed to setup_curl ...  */
oss_error_t ohttp_setup_curl(struct ohttp_connection *conn, enum ohttp_method method, struct oss_io *io)
{
    CURLcode curl_status;
    oss_error_t status = OSSE_UNKNOWN;
    
    conn->io = io;

    x_easy_setopt(CURLOPT_NOPROGRESS, 1);

    x_easy_setopt(CURLOPT_FOLLOWLOCATION, 1);

    x_easy_setopt(CURLOPT_USERAGENT, s_user_agent);
    
    x_easy_setopt(CURLOPT_HEADERDATA, conn);
    x_easy_setopt(CURLOPT_HEADERFUNCTION, &ohttp_curl_header_function);

    x_easy_setopt(CURLOPT_READDATA, conn);
    x_easy_setopt(CURLOPT_READFUNCTION, &ohttp_curl_read_function);

    x_easy_setopt(CURLOPT_WRITEDATA, conn);
    x_easy_setopt(CURLOPT_WRITEFUNCTION, &ohttp_curl_write_function);
    
    if ((status = ohttp_curl_set_headers(conn)) != OSSE_OK) {
        ologe("curl set headders failed");
        return status;
    }

    x_easy_setopt(CURLOPT_URL, conn->request.url);

    switch (method) {
        case OMETHOD_DELETE:
            x_easy_setopt(CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
            
        case OMETHOD_HEAD:
            x_easy_setopt(CURLOPT_NOBODY, 1);
            break;
            
        case OMETHOD_GET:
            break;

        case OMETHOD_PUT:
            x_easy_setopt(CURLOPT_UPLOAD, 1);
            break;
        default:
            return OSSE_IMPOSSIBLE;
    }

    curl_status = curl_easy_perform(conn->curl);
    if (curl_status != CURLE_OK) {
        ologe("curl status is %d\n", curl_status);
        return OSS_CURLE(curl_status);
    }

    set_response_status(conn);

    if (ohttp_conn_io_has(conn, end_recv_bdoy))
        status = conn->io->end_recv_bdoy(conn->io);
    
    return status;
}

oss_error_t ohttp_request_make_date(struct ohttp_request *request)
{
    char date_string[64];
    time_t now = time(NULL);
    strftime(date_string, sizeof(date_string), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
    
    if (!oss_kvlist_append(&request->headers, "date", date_string))
        return OSSE_NO_MEMORY;

    return OSSE_OK;
}

int is_http_success(int status)
{
    return status / 100 == 2;
}

oss_error_t ohttp_request(struct ohttp_connection *conn, 
                          enum ohttp_method method,
                          const char *bucket,
                          const char *object,
                          struct oss_io *io)
{
    oss_error_t status;
    struct ohttp_request *request = &conn->request;
    
    request->method = method;

    if ((status = ohttp_request_make_date(request)) != OSSE_OK) {
        ologe("failed to make date: %d", status);
        return status;
    }

    if ((status = ohttp_make_url(conn, bucket, object)) != OSSE_OK) {
        ologe("failed to make url");
        return status;
    }

    oss_kvlist_isort(&request->headers);
    oss_kvlist_sort(&request->queries);

    if ((status = ohttp_make_auth_header(request, &conn->cred, bucket, object)) != OSSE_OK) {
        ologe("failed to make auth header");
        return status;
    }

    if ((status = ohttp_setup_curl(conn, method, io)) != OSSE_OK) {
        ologe("failed to setup curl");
        return status;
    }

    if (!is_http_success(status)) {
        ologd("http error: %d", status);
        return status;
    }

    return OSSE_OK;
}
