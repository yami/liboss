#include "object.h"

#include "logging.h"
#include "xml.h"

#include <stdio.h>

oss_error_t oss_head_object(struct ohttp_connection *conn,
                            const char *bucket,
                            const char *object)
{
    enum oss_error status = OSSE_OK;

    if ((status = ohttp_request(conn, OMETHOD_HEAD, bucket, object)) != OSSE_OK) {
        ologe("oss_head_object failed to request");
        return status;
    }

    ologd("oss_head_object status = %d", conn->response.status);

    return status;
}


oss_i64 ohttp_fio_on_recv_body(char *p, size_t len, void *self)
{
    struct ohttp_fio *io = (struct ohttp_fio *) self;

    if (fwrite(p, 1, len, io->fh) == len)
        return len;
    else
        return 0;
}

oss_i64 ohttp_fio_on_send_body(char *p, size_t len, void *self)
{
    struct ohttp_fio *io = (struct ohttp_fio *) self;
    size_t nr_read = fread(p, 1, len, io->fh);

    if (nr_read == len)
        return nr_read;

    if (feof(io->fh))
        return 0;

    return -1;
}

struct ohttp_fio *ohttp_fread_create(FILE *fh)
{
    struct ohttp_fio *io = (struct ohttp_fio *) calloc(1, sizeof(*io));

    if (!io)
        return NULL;

    io->super.on_send_body = &ohttp_fio_on_send_body;
    io->fh = fh;

    return io;
}

struct ohttp_fio *ohttp_fwrite_create(FILE *fh)
{
    struct ohttp_fio *io = calloc(1, sizeof(*io));

    if (!io)
        return NULL;

    io->super.on_recv_body = &ohttp_fio_on_recv_body;
    io->fh = fh;

    return io;
}

oss_error_t oss_get_object_to_file(struct ohttp_connection *conn,
                                   const char *bucket,
                                   const char *object,
                                   const char *filename)
{
    FILE *fh;
    struct ohttp_fio *io = NULL;
    
    oss_error_t status = OSSE_OK;
    
    if (!(fh = fopen(filename, "wb"))) {
        ologe("failed to open for writing: %s", filename);

        return OSSE_OPEN_FILE;
    }

    if (!(io = ohttp_fwrite_create(fh))) {
        ologe("failed to alloc for io");
        
        fclose(fh);
        return OSSE_NO_MEMORY;
    }
    
    ohttp_set_io(conn, (struct ohttp_io *) io);

    if ((status = ohttp_request(conn, OMETHOD_GET, bucket, object)) != OSSE_OK) {
        ologe("failed to ohttp_request: %d", status);
    }
    
    fclose(fh);

    return status;
}

/* TODO: MING: avoid memcpy for input buffer? */
oss_error_t oss_put_object_from_buffer(struct ohttp_connection *conn,
                                       const char *bucket,
                                       const char *object,
                                       const void *buffer,
                                       int length)
{
    oss_error_t status = OSSE_OK;
    struct ohttp_memio *io = NULL;
    char fsize_str[256];

    snprintf(fsize_str, sizeof(fsize_str), "%d", length);

    if (!oss_kvlist_append(&conn->request.headers, "Content-Length", fsize_str)) {
        ologe("failed to append content-length");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    /* disable chunked encoding. TODO: Perhaps we should disable it inside ohttp_request() */
    if (!oss_kvlist_append(&conn->request.headers, "Transfer-Encoding", "")) {
        ologe("failed to disable chunked encoding");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    if (!(io = ohttp_memio_send_create(buffer, length))) {
        ologe("failed to create memio for send");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    ohttp_set_io(conn, (struct ohttp_io *) io);

    status = ohttp_request(conn, OMETHOD_PUT, bucket, object);
    if (status != OSSE_OK) {
        ologe("failed to ohttp_request, status=%d", status);
        goto error;
    }

error:
    return status;
}

/* http://en.wikipedia.org/wiki/Large_file_support */
oss_error_t oss_put_object_from_file(struct ohttp_connection *conn,
                                     const char *bucket,
                                     const char *object,
                                     const char *filename)
{
    FILE *fh = NULL;

    oss_i64 fsize;
    char fsize_str[256];
    
    struct ohttp_fio *io = NULL;
    oss_error_t status = OSSE_OK;

    if (!(fh = fopen(filename, "rb"))) {
        ologe("failed to open for read: %s", filename);
        status = OSSE_OPEN_FILE;
        goto error;
    }

    if ((fsize = oss_fsize(fh)) < 0) {
        ologe("failed to get file size: %s", filename);
        status = OSSE_IO_ERROR;
        goto error;
    }

    snprintf(fsize_str, sizeof(fsize_str), "%lld", fsize);
    
    if (!oss_kvlist_append(&conn->request.headers, "Content-Length", fsize_str)) {
        ologe("failed to append content-length");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    if (!(io = ohttp_fread_create(fh))) {
        ologe("failed to alloc stdio");
        status = OSSE_NO_MEMORY;
        goto error;
    }

    ohttp_set_io(conn, (struct ohttp_io *) io);
    
    if ((status = ohttp_request(conn, OMETHOD_PUT, bucket, object)) != OSSE_OK) {
        ologe("failed to ohttp_request: %d", status);
    }

    fclose(fh);

    return status;

error:
    if (fh)
        fclose(fh);

    return status;
}


oss_error_t oss_delete_object(struct ohttp_connection *conn,
                              const char *bucket,
                              const char *object)
{
    return ohttp_request(conn, OMETHOD_DELETE, bucket, object);
}


oss_error_t init_upload(struct ohttp_connection *conn,
                        const char *bucket,
                        const char *object,
                        char **upload_id)
{
    oss_error_t status = OSSE_OK;
    struct ohttp_request *request = &conn->request;
    struct ohttp_memio *io = NULL;

    *upload_id = NULL;
    
    if (!oss_kvlist_append(&request->queries, "uploads", "")) {
        ologe("failed to append uploads");
        return OSSE_NO_MEMORY;
    }

    if (!(io = ohttp_memio_recv_create())) {
        ologe("failed to create memout");
        return OSSE_NO_MEMORY;
    }

    ohttp_set_io(conn, (struct ohttp_io *)io);

    status = ohttp_request(conn, OMETHOD_POST, bucket, object);
    if (status != OSSE_OK) {
        ologe("failed to ohttp_request, status=%d", status);
        return status;
    }

    status = parse_init_upload_response(io->recv_buffer.data, io->recv_buffer.n, upload_id);
    if (status != OSSE_OK) {
        ologe("failed to parse response");
        return status;
    }

    return OSSE_OK;
}
