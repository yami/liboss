#include "util.h"

#include <ctype.h>
#include <stdlib.h>

#define next_alloc(x) (((x)+16)*3/2)

static
int grow_alloc(void **p, size_t member_size, int nalloc, int nreserve)
{
    if (next_alloc(nalloc) < nreserve)
        nalloc = nreserve;
    else
        nalloc = next_alloc(nalloc);
    
    *p = realloc(*p, nalloc * member_size);

    return (*p) ? nalloc : 0;
}

void str_tolower(char *str)
{
    char *p;
    
    for (p = str; *p; p++)
        *p = tolower(*p);
}

char *str_duplower(const char *s)
{
    char *dup = strdup(s);

    if (!dup)
        return NULL;

    str_tolower(dup);

    return dup;
}


int istarts_with(const char *str, const char *pat)
{
    int str_len = strlen(str);
    int pat_len = strlen(pat);

    if (pat_len > str_len)
        return 0;

    return strncasecmp(str, pat, pat_len) ? 0 : 1;
}

int starts_with(const char *str, const char *pat)
{
    int str_len = strlen(str);
    int pat_len = strlen(pat);

    if (pat_len > str_len)
        return 0;

    return strncmp(str, pat, pat_len) ? 0 : 1;
}


/* if s_needs_url_encoding[c] is 1, c needs url encoding */
const static unsigned char s_needs_url_encoding[] = {
    /*   0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /*  10 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /*  20 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /*  30 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /*  40 */ 1, 1, 1, 1, 1, 0, 0, 1, 0, 0,
    /*  50 */ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    /*  60 */ 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    /*  70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*  80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*  90 */ 0, 1, 1, 1, 1, 0, 1, 0, 0, 0,
    /* 100 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 110 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 120 */ 0, 0, 0, 1, 1, 1, 0, 1, 1, 1,
    /* 130 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 140 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 150 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 160 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 170 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 180 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 190 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 200 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 210 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 220 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 230 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 240 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 250 */ 1, 1, 1, 1, 1, 1,
};

char *url_encode(const void *str)
{
    /* Count new buffer length. Initialize len to 1 for terminated zero. */
    int len = 1;
    const unsigned char *p;
    char *encoded;
    char *current;

    for (p = (const unsigned char *) str; *p; p++) {
        if (s_needs_url_encoding[*p])
            len += 3;
        else
            len += 1;
    }

    encoded = (char *) malloc(len);
    if (!encoded)
        return NULL;

    current = encoded;
    for (p = (const unsigned char *) str; *p; p++) {
        if (s_needs_url_encoding[*p])
            current += sprintf(current, "%%%02X", *p);
        else
            *(current++) = *p;
    }

    *current = '\0';

    return encoded;
}

struct oss_kvlist * oss_kvlist_init(struct oss_kvlist *kvlist, int nreserve)
{
    void *ptr;

    memset(kvlist, 0, sizeof(*kvlist));

    if (nreserve <= 0)
        return kvlist;
    
    ptr = calloc(nreserve, sizeof(struct oss_kv));
    if (!ptr)
        return NULL;

    kvlist->items = (struct oss_kv *) ptr;
    kvlist->n = 0;
    kvlist->nalloc = nreserve;

    return kvlist;
}

static
void oss_kvlist_free_kv(struct oss_kvlist *kvlist)
{
    int i;
    for (i = 0; i < kvlist->n; i++) {
        struct oss_kv *kv = &kvlist->items[i];
        free(kv->key);
        free(kv->value);
    }
}

void oss_kvlist_free(struct oss_kvlist *kvlist)
{
    if (!kvlist || kvlist->nalloc == 0)
        return;

    oss_kvlist_free_kv(kvlist);
    free(kvlist->items);
}

void oss_kvlist_clear(struct oss_kvlist *kvlist)
{
    if (!kvlist || kvlist->nalloc == 0)
        return;
    
    oss_kvlist_free_kv(kvlist);
    
    memset(kvlist->items, 0, kvlist->nalloc * sizeof(kvlist->items[0]));
    kvlist->n = 0;
}

struct oss_kv * oss_kvlist_ifind(struct oss_kvlist *kvlist, const char *key)
{
    int i;

    for (i = 0; i < kvlist->n; i++) {
        if (strcasecmp(kvlist->items[i].key, key) == 0)
            return &kvlist->items[i];
    }

    return NULL;
}

static
int kvlist_icmp_key(const void *pa, const void *pb)
{
    struct oss_kv *a = (struct oss_kv *) pa;
    struct oss_kv *b = (struct oss_kv *) pb;

    return strcasecmp(a->key, b->key);
}

static
int kvlist_cmp_key(const void *pa, const void *pb)
{
    struct oss_kv *a = (struct oss_kv *) pa;
    struct oss_kv *b = (struct oss_kv *) pb;

    return strcmp(a->key, b->key);
}

void oss_kvlist_isort(struct oss_kvlist *kvlist)
{
    qsort(kvlist->items, kvlist->n, sizeof(*kvlist->items), kvlist_icmp_key);
}

void oss_kvlist_sort(struct oss_kvlist *kvlist)
{
    qsort(kvlist->items, kvlist->n, sizeof(*kvlist->items), kvlist_cmp_key);
}

static
void * free2(void *a, void *b)
{
    free(a);
    free(b);

    return NULL;
}

struct oss_kv * oss_kvlist_append(struct oss_kvlist *kvlist, const char *key, const char *value)
{
    struct oss_kv *item;
    char *k = strdup(key);
    char *v = strdup(value);

    if (!k || !v) {
        return free2(k, v);
    }

    item = oss_kvlist_append_nodup(kvlist, k, v);
    if (!item) {
        return free2(k, v);
    }

    return item;
}

struct oss_kv * oss_kvlist_append_nodup_v(struct oss_kvlist *kvlist, const char *key, char *value)
{
    struct oss_kv *item;
    char *k = strdup(key);
    if (!k)
        return NULL;

    item = oss_kvlist_append_nodup(kvlist, k, value);
    if (!item) 
        free(k);

    return item;
}

struct oss_kv * oss_kvlist_append_nodup(struct oss_kvlist *kvlist, char *key, char *value)
{
    struct oss_kv *item = NULL;
    
    int nreserve = kvlist->n + 1;
    int nalloc = kvlist->nalloc;
    
    if (nreserve > nalloc) {
        kvlist->nalloc = grow_alloc(&kvlist->items, sizeof(*kvlist->items), nalloc, nreserve);
        
        if (!kvlist->nalloc)
            return NULL;
    }

    item = &kvlist->items[kvlist->n++];
    item->key = key;
    item->value = value;

    return item;
}

struct oss_buffer * oss_buffer_init(struct oss_buffer *buffer, int nreserve)
{
    void *ptr = NULL;
    
    memset(buffer, 0, sizeof(*buffer));

    if (nreserve <= 0)
        return buffer;

    ptr = malloc(nreserve);
    if (!ptr)
        return NULL;

    buffer->data = ptr;
    buffer->n = 0;
    buffer->nalloc = nreserve;

    return buffer;
}

static void *memdup(const void *p, int size)
{
    void *dup = malloc(size);
    if (!dup)
        return NULL;

    memcpy(dup, p, size);

    return dup;
}

struct oss_buffer *oss_buffer_assign(struct oss_buffer *buffer, const void *ptr, int size)
{
    void *dup = memdup(ptr, size);
    if (!dup)
        return NULL;

    oss_buffer_free(buffer);

    buffer->data = dup;
    buffer->n = size;
    buffer->nalloc = size;

    return buffer;
}

void oss_buffer_free(struct oss_buffer *buffer)
{
    free(buffer->data);
    oss_buffer_detach(buffer);
}

void oss_buffer_clear(struct oss_buffer *buffer)
{
    if (!buffer || buffer->nalloc == 0)
        return;

    memset(buffer->data, 0, buffer->nalloc);
    buffer->n = 0;
}

void oss_buffer_detach(struct oss_buffer *buffer)
{
    oss_buffer_init(buffer, 0);
}

char * oss_buffer_detach_as_string(struct oss_buffer *buffer)
{
    char *string = NULL;
    
    if (!oss_buffer_append_zero(buffer)) {
        oss_buffer_free(buffer);
        return NULL;
    }

    string = buffer->data;
    oss_buffer_detach(buffer);

    return string;
}

char * oss_buffer_append(struct oss_buffer *buffer, const char *idata, int ilen)
{
    int nreserve = buffer->n + ilen;
    int nalloc = buffer->nalloc;
    char *data = buffer->data;

    if (nreserve > nalloc) {
        nalloc = grow_alloc(&data, 1, nalloc, nreserve);

        if (!nalloc)
            return NULL;

        buffer->nalloc = nalloc;
        buffer->data = data;
    }

    memcpy(buffer->data + buffer->n, idata, ilen);
    buffer->n += ilen;
    
    return buffer->data + (buffer->n - ilen);
}

char * oss_buffer_append_zero(struct oss_buffer *buffer)
{
    char zero = '\0';
    return oss_buffer_append(buffer, &zero, 1);
}

char * oss_buffer_back(struct oss_buffer *buffer)
{
    return &buffer->data[buffer->n - 1];
}


oss_i64 oss_fsize(FILE* fh)
{
    oss_i64 size = -1;
    oss_i64 old_offset = _ftelli64(fh);
    
    if (oss_fseek(fh, 0, SEEK_END) != 0)
        return -1;
    
    size = oss_ftell(fh);
    
    if (oss_fseek(fh , old_offset, SEEK_SET) != 0)
        return -1;
    
    return size;
}
