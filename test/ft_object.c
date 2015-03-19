#include "unity.h"

#include "ftutil.h"
#include "object.h"

static struct ft_config *config = NULL;

void setUp()
{
}

void tearDown()
{
}

void test_PutObjectFromBuffer(void)
{
    oss_error_t status = OSSE_OK;
    char *object = make_object_name(config->prefix);
    
    const char *buffer = "test content";
    int length = strlen(buffer);
    
    struct ohttp_connection *conn = NULL;

    conn = ohttp_connection_create(config->region, config->host, config->port);

    ohttp_set_credential(conn, config->access_id, config->access_key);
    
    status = oss_put_object_from_buffer(conn, config->bucket, object, buffer, length);
    TEST_ASSERT_EQUAL(OSSE_OK, status);

    ohttp_connection_clear(conn);

    status = oss_head_object(conn, config->bucket, object);
    TEST_ASSERT_EQUAL(OSSE_OK, status);
}

int main(int argc, char **argv)
{
    ohttp_init();
    
    config = ft_config_parse(argc, argv);
    if (!config)
        return 1;

    UnityBegin();

    RUN_TEST(test_PutObjectFromBuffer, __LINE__);

    UnityEnd();

    return 0;
}
