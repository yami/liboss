#include "object.h"

#include "logging.h"

#include <stdio.h>

oss_error_t oss_head_object(struct ohttp_connection *conn,
                            const char *bucket,
                            const char *object)
{
    enum oss_error status = OSSE_OK;

    if ((status = ohttp_request(conn, OMETHOD_HEAD, bucket, object, NULL)) != OSSE_OK) {
        ologe("oss_head_object failed to request");
        return status;
    }

    ologd("oss_head_object status = %d", conn->response.status);

    return status;
}

struct oss_fio {
    struct oss_io super;

    FILE *fh;
};

oss_i64 oss_fio_on_recv_body(char *p, size_t len, void *user_data)
{
    struct oss_fio *io = (struct oss_fio *) user_data;

    if (fwrite(p, 1, len, io->fh) == len)
        return len;
    else
        return 0;
}

oss_i64 oss_fio_on_send_body(char *p, size_t len, void *user_data)
{
    struct oss_fio *io = (struct oss_fio *) user_data;
    size_t nr_read = fread(p, 1, len, io->fh);

    if (nr_read == len)
        return nr_read;

    if (feof(io->fh))
        return 0;

    return -1;
}

struct oss_io *oss_fread_create(FILE *fh)
{
    struct oss_fio *io = (struct oss_fio *) calloc(1, sizeof(*io));

    if (!io)
        return NULL;

    io->super.on_send_body = &oss_fio_on_send_body;
    io->fh = fh;

    return (struct oss_io *) io;
}

struct oss_io *oss_fwrite_create(FILE *fh)
{
    struct oss_fio *io = calloc(1, sizeof(*io));

    if (!io)
        return NULL;

    io->super.on_recv_body = &oss_fio_on_recv_body;
    io->fh = fh;

    return (struct oss_io *) io;
}

oss_error_t oss_get_object_to_file(struct ohttp_connection *conn,
                                   const char *bucket,
                                   const char *object,
                                   const char *filename)
{
    FILE *fh;
    struct oss_io *io = NULL;
    
    oss_error_t status = OSSE_OK;
    
    if (!(fh = fopen(filename, "wb"))) {
        ologe("failed to open for writing: %s", filename);

        return OSSE_OPEN_FILE;
    }

    if (!(io = oss_fwrite_create(fh))) {
        ologe("failed to alloc for io");
        
        fclose(fh);
        return OSSE_NO_MEMORY;
    }
    

    if ((status = ohttp_request(conn, OMETHOD_GET, bucket, object, io)) != OSSE_OK) {
        ologe("failed to ohttp_request: %d", status);
    }
    
    fclose(fh);

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
    
    struct oss_io *io = NULL;
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

    if (!(io = oss_fread_create(fh))) {
        ologe("failed to alloc stdio");
        status = OSSE_NO_MEMORY;
        goto error;
    }
    
    if ((status = ohttp_request(conn, OMETHOD_PUT, bucket, object, io)) != OSSE_OK) {
        ologe("failed to ohttp_request: %d", status);
    }

    fclose(fh);

    return status;

error:
    free(io);

    if (fh)
        fclose(fh);

    return status;
}


oss_error_t oss_delete_object(struct ohttp_connection *conn,
                              const char *bucket,
                              const char *object)
{
    return ohttp_request(conn, OMETHOD_DELETE, bucket, object, NULL);
}
