#include "ftutil.h"

#include "logging.h"
#include "util.h"
#include "error_code.h"
#include "xml.h"
#include "asprintf.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdlib.h>
#include <stdio.h>

void ft_config_free(struct ft_config *config)
{
    if (!config)
        return;
    
    free(config->region);
    free(config->host);

    free(config->access_id);
    free(config->access_key);

    free(config->bucket);
    free(config->prefix);

    free(config);
}

int ft_config_required_set(struct ft_config *config)
{
    return
        config->region &&
        config->host &&
        config->access_id &&
        config->access_key &&
        config->bucket &&
        config->prefix;
}

char *read_file_content(const char *filename, int *size)
{
    int file_size;
    char *file_content = NULL;
    FILE *fh = NULL;
    
    *size = 0;

    fh = fopen(filename, "rb");
    if (!fh) {
        ologe("failed to open file: %s", filename);
        goto error;
    }
    
    file_size = (int) oss_fsize(fh);
    if (file_size == -1) {
        ologe("failed to obtain file size");
        goto error;
    }
    
    file_content = malloc(file_size + 1);
    if (!file_content) {
        ologe("failed to malloc file_content");
        goto error;
    }
    
    if (file_size != fread(file_content, 1, file_size, fh)) {
        ologe("failed to read file");
        goto error;
    }

    fclose(fh);
    
    *size = file_size;
    return file_content;
    
error:
    free(file_content);
    
    if (fh)
        fclose(fh);

    return NULL;
}

int parse_opt_string(const char *arg, const char *opt, char **out)
{
    if (starts_with(arg, opt)) {
        if (!(*out))
            *out = strdup(arg + strlen(opt));

        return 1;
    } else {
        return 0;
    }
}

int parse_opt_int(const char *arg, const char *opt, int *out)
{
    if (starts_with(arg, opt)) {
        if (!(*out))
            *out = atoi(arg + strlen(opt));
        
        return 1;
    } else {
        return 0;
    }
}

void parse_config_options(int argc, char **argv, struct ft_config *config, const char **config_file)
{
    int i;
    
    for (i = 1; i < argc; ++i) {
        if (parse_opt_string(argv[i], "--region=", &config->region))
            continue;

        if (parse_opt_string(argv[i], "--host=", &config->host))
            continue;

        if (parse_opt_string(argv[i], "--access_id=", &config->access_id))
            continue;

        if (parse_opt_string(argv[i], "--access_key=", &config->access_key))
            continue;

        if (parse_opt_string(argv[i], "--bucket=", &config->bucket))
            continue;
        
        if (parse_opt_string(argv[i], "--prefix=", &config->prefix))
            continue;
            
        if (parse_opt_int(argv[i], "--port=", &config->port))
            continue;

        /* It must be a config file name. Options after it should be
         * discarded. */

        *config_file = argv[i];
    }
}

static
oss_error_t ft_action_get_string(char *value, void *user_data)
{
    char **p = (char **) user_data;

    if (*p == NULL) {
        *p = value;
    } else {
        free(value);
    }
    
    return OSSE_OK;
}

static
oss_error_t ft_action_get_int(char *value, void *user_data)
{
    int *p = (int *) user_data;

    if (*p == 0) {
        *p = atoi(value);
    }

    free(value);

    return OSSE_OK;
}

int parse_config_file(const char *config_file, struct ft_config *config)
{
    int success = 0;
    
    char *file_content = NULL;
    int file_size;
    
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;

    struct parse_key_action actions[] = {
        {"Region",    ft_action_get_string, &config->region},
        {"Host",      ft_action_get_string, &config->host},
        {"AccessId",  ft_action_get_string, &config->access_id},
        {"AccessKey", ft_action_get_string, &config->access_key},
        {"Bucket",    ft_action_get_string, &config->bucket},
        {"Prefix",    ft_action_get_string, &config->prefix},
        {"Port",      ft_action_get_int,    &config->port},
        {NULL}
    };

    
    file_content = read_file_content(config_file, &file_size);

    if (!file_content)
        goto error;

    if (xml_get_doc_root(file_content, file_size, "FunctionalTestConfig", &doc, &root) != OSSE_OK)
        goto error;

    if (parse_kvlist(root, actions) != OSSE_OK)
        goto error;
    
    success = 1;
error:
    free(file_content);
    
    if (doc)
        xmlFreeDoc(doc);

    return success;
}

struct ft_config *ft_config_create()
{
    return calloc(1, sizeof(struct ft_config));
}

struct ft_config *ft_config_parse(int argc, char **argv)
{
    const char *config_file = "ft_config.xml";
    struct ft_config *config = NULL;

    if (!(config = ft_config_create())) {
        ologe("failed to create ft_config");
        return NULL;
    }

    parse_config_options(argc, argv, config, &config_file);

    if (ft_config_required_set(config)) {
        config->port = config->port ? config->port : 0;
        return config;
    }
    
    if (!parse_config_file(config_file, config)) {
        ft_config_free(config);
        return NULL;
    }

    if (config->port == 0)
        config->port = 80;

    if (!ft_config_required_set(config)) {
        ologe("missing configuration");
        ft_config_free(config);
        return NULL;
    }

    return config;
}

char *make_object_name2(const char* prefix1, const char* prefix2)
{
    static int s_index = 0;
    char *name = NULL;
    
    asprintf(&name, "%s%s-%d", prefix1, prefix2, s_index++);

    return name;
}
