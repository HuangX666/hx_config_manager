/* test_json.c */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const char *JSON_DATA =
    "{"
    "  \"server\": {"
    "    \"host\": \"localhost\","
    "    \"port\": 3000,"
    "    \"ssl\":  true"
    "  },"
    "  \"tags\": [\"web\", \"api\"],"
    "  \"ratio\": 1.5,"
    "  \"empty\": null"
    "}";

int main(void)
{
    printf("=== test_json ===\n");
    cm_ctx_t *ctx = cm_ctx_create();

    assert(cm_load_string(ctx, JSON_DATA, CM_FORMAT_JSON) == CM_OK);

    const char *host = NULL;
    assert(cm_get_string(ctx, "server.host", &host) == CM_OK);
    assert(strcmp(host, "localhost") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx, "server.port", &port) == CM_OK);
    assert(port == 3000);

    int ssl = 0;
    assert(cm_get_bool(ctx, "server.ssl", &ssl) == CM_OK);
    assert(ssl == 1);

    double ratio = 0;
    assert(cm_get_float(ctx, "ratio", &ratio) == CM_OK);
    assert(ratio > 1.4 && ratio < 1.6);

    size_t len = 0;
    assert(cm_array_length(ctx, "tags", &len) == CM_OK);
    assert(len == 2);

    /* round-trip */
    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_JSON, &olen);
    assert(out && olen > 0);
    free(out);

    assert(cm_load_string(ctx, "{ invalid", CM_FORMAT_JSON) == CM_ERR_PARSE);
    host = NULL;
    assert(cm_get_string(ctx, "server.host", &host) == CM_OK);
    assert(strcmp(host, "localhost") == 0);

    cm_ctx_destroy(ctx);
    printf("  JSON load/save             PASS\n");
    return 0;
}
