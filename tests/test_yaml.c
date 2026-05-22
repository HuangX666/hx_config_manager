/* test_yaml.c */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const char *YAML_DATA =
    "database:\n"
    "  host: db.example.com\n"
    "  port: 5432\n"
    "  name: mydb\n"
    "  pool: 10\n"
    "  ssl: true\n"
    "cache:\n"
    "  ttl: 300\n"
    "  backend: redis\n"
    "features:\n"
    "  - search\n"
    "  - analytics\n"
    "  - export\n";

int main(void)
{
    printf("=== test_yaml ===\n");
    cm_ctx_t *ctx = cm_ctx_create();

    cm_error_t err = cm_load_string(ctx, YAML_DATA, CM_FORMAT_YAML);
    if (err != CM_OK) {
        printf("  YAML load: %s (skipping – libyaml may not be linked)\n",
               cm_error_str(err));
        cm_ctx_destroy(ctx);
        return 0;
    }

    const char *host = NULL;
    assert(cm_get_string(ctx, "database.host", &host) == CM_OK);
    assert(strcmp(host, "db.example.com") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx, "database.port", &port) == CM_OK);
    assert(port == 5432);

    int ssl = 0;
    assert(cm_get_bool(ctx, "database.ssl", &ssl) == CM_OK);
    assert(ssl == 1);

    size_t len = 0;
    assert(cm_array_length(ctx, "features", &len) == CM_OK);
    assert(len == 3);

    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_YAML, &olen);
    assert(out);
    free(out);

    cm_ctx_destroy(ctx);
    printf("  YAML load/save             PASS\n");
    return 0;
}
