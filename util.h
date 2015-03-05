#ifndef OSS_UTIL_H
#define OSS_UTIL_H

#include <string.h>
#include <stdio.h>

/* platform dependent */

/* Since Visual Studio 2005, MSVC supports 'long long', so we use
 * 'long long' as our 64 bits integer. So we do not support MSVC older
 * than VS 2005.
 *
 * The good thing is we can use '%lld' to print oss_i64.
 */

#ifdef _WIN32                   /* TODO: should we use MS VC like macro instead of win32? */
    typedef long long oss_i64;

    #define oss_fseek _fseeki64
    #define oss_ftell _ftelli64
#else
    typedef long long oss_i64;

    #define oss_fseek fseeko
    #define oss_ftell ftello
#endif

/* stdio */

/* get file size given FILE stream. */
oss_i64 oss_fsize(FILE* fh);

/* string */
#ifdef _WIN32
    #define strdup _strdup
    #define strcasecmp _stricmp 
    #define strncasecmp _strnicmp
    #define snprintf _snprintf
#else
    #include <strings.h>
#endif

void str_tolower(char *str);
char *str_duplower(const char *s);
int istarts_with(const char *str, const char *pat);
char *url_encode(const void *str);

/* data structures defined by this lib. */

struct oss_kv {
    char *key;
    char *value;
};

struct oss_kvlist {
    struct oss_kv *items;
    int n;                      /* number of items user can see */
    int nalloc;                 /* number of items allocated */
};

#define OSS_KVLIST_INIT { NULL, 0, 0 }

struct oss_kvlist * oss_kvlist_init(struct oss_kvlist *kvlist, int nreserve);
void oss_kvlist_free(struct oss_kvlist *kvlist);

struct oss_kv * oss_kvlist_append(struct oss_kvlist *kvlist, const char *key, const char *value);
struct oss_kv * oss_kvlist_append_nodup(struct oss_kvlist *kvlist, char *key, char *value);

struct oss_kv * oss_kvlist_ifind(struct oss_kvlist *kvlist, const char *key);
void oss_kvlist_isort(struct oss_kvlist *kvlist);
void oss_kvlist_sort(struct oss_kvlist *kvlist);

struct oss_buffer {
    char *data;                 /* may or may not be NULL/zero-terminated */
    int n;                      /* number of bytes user can see */
    int nalloc;                 /* number of bytes allocated */
};

#define OSS_BUFFER_INIT {NULL, 0, 0}

struct oss_buffer * oss_buffer_init(struct oss_buffer *buffer, int nreserve);
void oss_buffer_free(struct oss_buffer *buffer);

void oss_buffer_detach(struct oss_buffer *buffer);
char * oss_buffer_detach_as_string(struct oss_buffer *buffer);

char * oss_buffer_append(struct oss_buffer *buffer, const char *data, int len);
char * oss_buffer_append_zero(struct oss_buffer *buffer);

char * oss_buffer_back(struct oss_buffer *buffer);

#endif
