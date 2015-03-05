#include "bucket.h"

#include "xml.h"

#include "asprintf.h"
#include "logging.h"

#include <time.h>



oss_error_t oss_get_service(struct ohttp_connection *conn, struct oss_service *service)
{
    struct oss_memio *io = NULL;
    oss_error_t status = OSSE_UNKNOWN;
    
    if ((status = ohttp_request(conn, OMETHOD_GET, NULL, NULL, oss_memout_create())) != OSSE_OK) {
        ologe("failed to get service");
        return status;
    }

    if (conn->response.status != 200) {
        ologe("http error: %s", conn->response.ebody);
        return conn->response.status;
    }

    io = (struct oss_memio *) conn->io;
    if (io->recv_buffer.n <= 0) {
        ologe("empty recv buffer");
        return OSSE_IMPOSSIBLE;
    }

    status = parse_get_service_response(io->recv_buffer.data, io->recv_buffer.n, service);
    if (status != OSSE_OK) {
        ologe("failed to parse response");
        return status;
    }

    ologi("id: %s", service->owner.id);
    ologi("display_name: %s", service->owner.display_name);

    return OSSE_OK;
}


oss_error_t oss_get_bucket_acl(struct ohttp_connection *conn, const char *bucket, struct oss_bucket_acl *acl)
{
    oss_error_t status = OSSE_OK;
    
    struct ohttp_request *request = &conn->request;
    struct oss_memio *io = NULL;

    if (!oss_kvlist_append(&request->queries, "acl", "")) {
        ologe("failed to append acl");
        return OSSE_NO_MEMORY;
    }

    status = ohttp_request(conn, OMETHOD_GET, bucket, NULL, oss_memout_create());
    if (status != OSSE_OK) {
        ologe("failed to ohttp_request, status=%d", status);
        return status;
    }

    io = (struct oss_memio *) conn->io;
    if (io->recv_buffer.n > 0) {
        ologe("recv buffer: %s\n", io->recv_buffer.data);
    }

    return OSSE_OK;
}


/* TODO: MING: check result of oss_memin_create, oss_memout_create_ */
oss_error_t oss_put_bucket_website(struct ohttp_connection *conn, const char *bucket, const char *index_file, const char *error_file)
{
    oss_error_t status = OSSE_OK;
    struct ohttp_request *request = &conn->request;

    char content_length[256];
    
    int xml_size = 0;
    char *xml = create_website_xml(index_file, error_file, &xml_size);
    if (!xml) {
        ologe("failed to create xml");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    snprintf(content_length, sizeof(content_length), "%d", xml_size);

    if (!oss_kvlist_append(&request->queries, "website", "")) {
        ologe("failed to append website");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    if (!oss_kvlist_append(&request->headers, "Content-Length", content_length)) {
        ologe("failed to append content-length");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    status = ohttp_request(conn, OMETHOD_PUT, bucket, NULL, oss_memin_create(xml, xml_size));
    if (status != OSSE_OK) {
        ologe("failed to ohttp_request, status=%d", status);
        goto error;
    }

error:
    free_xml(xml);
    return OSSE_OK;
}
