#ifndef OSS_TEST_FTUTIL_H
#define OSS_TEST_FTUTIL_H

struct ft_config {
    char *region;
    
    char *host;
    int port;

    char *access_id;
    char *access_key;

    char *bucket;
    char *prefix;
};

struct ft_config *ft_config_parse(int argc, char **argv);
void ft_config_free(struct ft_config *config);

#define make_object_name(PREFIX) make_object_name2(PREFIX, __FUNCTION__)

char *make_object_name2(const char* prefix1, const char* prefix2);

#endif
