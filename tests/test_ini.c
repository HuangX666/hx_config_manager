/* test_ini.c */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const char *INI_DATA =
    "; Application config\n"
    "app_name = MyService\n"
    "log_level = info\n"
    "\n"
    "[database]\n"
    "host = db.local\n"
    "port = 5432\n"
    "name = production\n"
    "max_conn = 20\n"
    "ssl = true\n"
    "\n"
    "[cache]\n"
    "backend = redis\n"
    "ttl = 600\n"
    "max_size = 1024\n";

int main(void)
{
    printf("=== test_ini ===\n");
    cm_ctx_t *ctx = cm_ctx_create();

    assert(cm_load_string(ctx, INI_DATA, CM_FORMAT_INI) == CM_OK);

    const char *name = NULL;
    assert(cm_get_string(ctx, "app_name", &name) == CM_OK);
    assert(strcmp(name, "MyService") == 0);

    const char *host = NULL;
    assert(cm_get_string(ctx, "database.host", &host) == CM_OK);
    assert(strcmp(host, "db.local") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx, "database.port", &port) == CM_OK);
    assert(port == 5432);

    int ssl = 0;
    assert(cm_get_bool(ctx, "database.ssl", &ssl) == CM_OK);
    assert(ssl == 1);

    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_INI, &olen);
    assert(out);
    free(out);

    cm_ctx_destroy(ctx);
    printf("  INI load/save              PASS\n");
    return 0;
}
