/**
 * basic_usage.c  –  Quickstart example for config_manager
 *
 * Demonstrates: create context, set/get values, nested keys,
 *               arrays, save/load JSON, dump.
 */
#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    printf("=== config_manager basic usage ===\n\n");

    /* 1. Create a config context */
    cm_ctx_t *cfg = cm_ctx_create();

    /* 2. Set values of various types */
    cm_set_string(cfg, "app.name",    "MyService");
    cm_set_string(cfg, "app.version", "2.0.0");
    cm_set_bool  (cfg, "app.debug",   0);
    cm_set_int   (cfg, "app.workers", 4);

    cm_set_string(cfg, "server.host", "0.0.0.0");
    cm_set_int   (cfg, "server.port", 8080);
    cm_set_bool  (cfg, "server.ssl",  1);

    cm_set_string(cfg, "database.host",     "db.local");
    cm_set_int   (cfg, "database.port",     5432);
    cm_set_string(cfg, "database.name",     "production");
    cm_set_int   (cfg, "database.max_conn", 20);
    cm_set_float (cfg, "database.timeout",  30.5);

    cm_set_string(cfg, "logging.level", "info");
    cm_set_string(cfg, "logging.file",  "/var/log/app.log");
    cm_set_bool  (cfg, "logging.rotate",1);

    /* arrays */
    cm_array_push_string(cfg, "server.allowed_origins", "https://example.com");
    cm_array_push_string(cfg, "server.allowed_origins", "https://api.example.com");
    cm_array_push_string(cfg, "server.allowed_origins", "http://localhost:3000");

    cm_array_push_int(cfg, "app.ports", 8080);
    cm_array_push_int(cfg, "app.ports", 8081);
    cm_array_push_int(cfg, "app.ports", 8443);

    /* 3. Read values back */
    printf("--- Reading values ---\n");

    const char *app_name = cm_get_string_or(cfg, "app.name", "unknown");
    printf("  app.name     = %s\n", app_name);

    int64_t port = cm_get_int_or(cfg, "server.port", -1);
    printf("  server.port  = %lld\n", (long long)port);

    int ssl = cm_get_bool_or(cfg, "server.ssl", 0);
    printf("  server.ssl   = %s\n", ssl ? "true" : "false");

    double timeout = cm_get_float_or(cfg, "database.timeout", 0.0);
    printf("  db.timeout   = %.1f\n", timeout);

    /* 4. Array access */
    printf("\n--- Array access ---\n");
    size_t origins_len = 0;
    cm_array_length(cfg, "server.allowed_origins", &origins_len);
    printf("  allowed_origins count = %zu\n", origins_len);
    for (size_t i = 0; i < origins_len; i++) {
        cm_node_t *item = cm_array_get(cfg, "server.allowed_origins", i);
        if (item) printf("    [%zu] = %s\n", i, item->value.sval);
    }

    /* 5. has_key / delete */
    printf("\n--- has_key / delete ---\n");
    printf("  has app.workers?  %s\n", cm_has_key(cfg,"app.workers") ? "yes":"no");
    cm_delete(cfg, "app.workers");
    printf("  after delete?     %s\n", cm_has_key(cfg,"app.workers") ? "yes":"no");

    /* 6. Serialize to JSON */
    printf("\n--- JSON output ---\n");
    size_t json_len = 0;
    char *json_str = cm_save_string(cfg, CM_FORMAT_JSON, &json_len);
    if (json_str) {
        printf("%s\n", json_str);
        free(json_str);
    }

    /* 7. Debug dump */
    printf("\n--- Debug dump ---\n");
    cm_dump(cfg);

    cm_ctx_destroy(cfg);
    return 0;
}
