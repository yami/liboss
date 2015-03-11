#ifndef OSS_OBJECT_H
#define OSS_OBJECT_H

#include "http.h"

oss_error_t oss_head_object(struct ohttp_connection *conn,
                            const char *bucket,
                            const char *object);

oss_error_t oss_get_object_to_file(struct ohttp_connection *conn,
                                   const char *bucket,
                                   const char *object,
                                   const char *filename);

oss_error_t oss_put_object_from_file(struct ohttp_connection *conn,
                                     const char *bucket,
                                     const char *object,
                                     const char *filename);

oss_error_t oss_delete_object(struct ohttp_connection *conn,
                              const char *bucket,
                              const char *object);

oss_error_t init_upload(struct ohttp_connection *conn,
                        const char *bucket,
                        const char *object,
                        char **upload_id);

oss_error_t upload_part(struct ohttp_connection *conn,
                        const char *bucket,
                        const char *object,
                        const char *upload_id,
                        struct oss_io *io);
#endif
