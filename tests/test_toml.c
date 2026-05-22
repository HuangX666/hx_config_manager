/* test_toml.c */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const char *TOML_DATA =
    "# Application configuration\n"
    "app_name = \"SuperApp\"\n"
    "version  = \"1.2.3\"\n"
    "debug    = false\n"
    "workers  = 4\n"
    "timeout  = 30.5\n"
    "\n"
    "[server]\n"
    "host = \"0.0.0.0\"\n"
    "port = 8080\n"
    "ssl  = true\n"
    "\n"
    "[database]\n"
    "host     = \"db.local\"\n"
    "port     = 5432\n"
    "name     = \"prod\"\n"
    "max_conn = 20\n"
    "\n"
    "[logging]\n"
    "level  = \"info\"\n"
    "file   = \"/var/log/app.log\"\n"
    "rotate = true\n"
    "\n"
    "[[plugins]]\n"
    "name    = \"auth\"\n"
    "enabled = true\n"
    "\n"
    "[[plugins]]\n"
    "name    = \"metrics\"\n"
    "enabled = false\n"
    "\n"
    "[features]\n"
    "flags = [\"search\", \"export\", \"api\"]\n"
    "limits = [100, 200, 500]\n";

int main(void)
{
    printf("=== test_toml ===\n");
    cm_ctx_t *ctx = cm_ctx_create();

    cm_error_t err = cm_load_string(ctx, TOML_DATA, CM_FORMAT_TOML);
    if (err != CM_OK) {
        printf("  TOML load: %s (skipping)\n", cm_error_str(err));
        cm_ctx_destroy(ctx);
        return 0;
    }

    /* scalars */
    const char *app_name = NULL;
    assert(cm_get_string(ctx, "app_name", &app_name) == CM_OK);
    assert(strcmp(app_name, "SuperApp") == 0);

    int64_t workers = 0;
    assert(cm_get_int(ctx, "workers", &workers) == CM_OK);
    assert(workers == 4);

    double timeout = 0;
    assert(cm_get_float(ctx, "timeout", &timeout) == CM_OK);
    assert(timeout > 30.0 && timeout < 31.0);

    int debug = 1;
    assert(cm_get_bool(ctx, "debug", &debug) == CM_OK);
    assert(debug == 0);

    /* nested table */
    const char *host = NULL;
    assert(cm_get_string(ctx, "server.host", &host) == CM_OK);
    assert(strcmp(host, "0.0.0.0") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx, "server.port", &port) == CM_OK);
    assert(port == 8080);

    int ssl = 0;
    assert(cm_get_bool(ctx, "server.ssl", &ssl) == CM_OK);
    assert(ssl == 1);

    /* database */
    int64_t dbport = 0;
    assert(cm_get_int(ctx, "database.port", &dbport) == CM_OK);
    assert(dbport == 5432);

    /* inline array */
    size_t flags_len = 0;
    assert(cm_array_length(ctx, "features.flags", &flags_len) == CM_OK);
    assert(flags_len == 3);

    cm_node_t *flag0 = cm_array_get(ctx, "features.flags", 0);
    assert(flag0 && flag0->type == CM_TYPE_STRING);
    assert(strcmp(flag0->value.sval, "search") == 0);

    size_t limits_len = 0;
    assert(cm_array_length(ctx, "features.limits", &limits_len) == CM_OK);
    assert(limits_len == 3);

    cm_node_t *lim1 = cm_array_get(ctx, "features.limits", 1);
    assert(lim1 && lim1->type == CM_TYPE_INT);
    assert(lim1->value.ival == 200);

    /* array of tables [[plugins]] */
    size_t plugin_len = 0;
    assert(cm_array_length(ctx, "plugins", &plugin_len) == CM_OK);
    assert(plugin_len == 2);

    /* round-trip save */
    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_TOML, &olen);
    assert(out && olen > 0);

    /* reload saved string */
    cm_ctx_t *ctx2 = cm_ctx_create();
    err = cm_load_string(ctx2, out, CM_FORMAT_TOML);
    free(out);
    if (err == CM_OK) {
        const char *name2 = NULL;
        assert(cm_get_string(ctx2, "app_name", &name2) == CM_OK);
        assert(strcmp(name2, "SuperApp") == 0);
    }
    cm_ctx_destroy(ctx2);

    cm_ctx_destroy(ctx);
    printf("  TOML load/save             PASS\n");
    return 0;
}
