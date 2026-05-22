/**
 * config_merge.c  –  Demonstrates layered configuration pattern:
 *
 *   defaults → base config → environment override → CLI override
 *
 * A common real-world pattern for 12-factor apps.
 */
#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Simulate four config sources ──────────────────────────────── */

/* Layer 0: Hard-coded defaults */
static void apply_defaults(cm_ctx_t *cfg)
{
    cm_set_string(cfg, "server.host",        "127.0.0.1");
    cm_set_int   (cfg, "server.port",        8080);
    cm_set_bool  (cfg, "server.ssl",         0);
    cm_set_int   (cfg, "server.workers",     2);

    cm_set_string(cfg, "database.host",      "localhost");
    cm_set_int   (cfg, "database.port",      5432);
    cm_set_string(cfg, "database.name",      "development");
    cm_set_int   (cfg, "database.pool",      3);

    cm_set_string(cfg, "cache.host",         "localhost");
    cm_set_int   (cfg, "cache.port",         6379);
    cm_set_int   (cfg, "cache.ttl",          300);

    cm_set_string(cfg, "logging.level",      "debug");
    cm_set_string(cfg, "logging.format",     "text");

    cm_set_bool  (cfg, "features.search",    0);
    cm_set_bool  (cfg, "features.analytics", 0);
    cm_set_bool  (cfg, "features.export",    0);
}

/* Layer 1: Base config file (JSON simulates a config.json) */
static const char *BASE_JSON =
    "{"
    "  \"server\": {"
    "    \"host\": \"0.0.0.0\","
    "    \"workers\": 4"
    "  },"
    "  \"database\": {"
    "    \"host\": \"db.staging.internal\","
    "    \"name\": \"staging\","
    "    \"pool\": 10"
    "  },"
    "  \"logging\": {"
    "    \"level\":  \"info\","
    "    \"format\": \"json\""
    "  },"
    "  \"features\": {"
    "    \"search\": true"
    "  }"
    "}";

/* Layer 2: Production environment file (.env) */
static const char *PROD_ENV =
    "SERVER_HOST=prod.example.com\n"
    "SERVER_PORT=443\n"
    "SERVER_SSL=true\n"
    "SERVER_WORKERS=16\n"
    "DATABASE_HOST=db.prod.cluster\n"
    "DATABASE_NAME=production\n"
    "DATABASE_POOL=50\n"
    "CACHE_HOST=redis.prod.cluster\n"
    "CACHE_TTL=3600\n"
    "LOGGING_LEVEL=warn\n"
    "FEATURE_ANALYTICS=true\n"
    "FEATURE_EXPORT=true\n";

/* Layer 3: CLI overrides (simulate: --server.port=8443 --logging.level=error) */
static void apply_cli_overrides(cm_ctx_t *cfg)
{
    cm_set_int   (cfg, "server.port",    8443);
    cm_set_string(cfg, "logging.level",  "error");
}

/* ─── Helpers ────────────────────────────────────────────────────── */

static void print_layer(cm_ctx_t *cfg, const char *layer_name)
{
    printf("\n[%s]\n", layer_name);
    printf("  server:   %s:%lld  ssl=%s  workers=%lld\n",
           cm_get_string_or(cfg, "server.host", "?"),
           (long long)cm_get_int_or(cfg, "server.port", 0),
           cm_get_bool_or  (cfg, "server.ssl",  0) ? "on" : "off",
           (long long)cm_get_int_or(cfg, "server.workers", 0));
    printf("  database: %s/%s  pool=%lld\n",
           cm_get_string_or(cfg, "database.host", "?"),
           cm_get_string_or(cfg, "database.name", "?"),
           (long long)cm_get_int_or(cfg, "database.pool", 0));
    printf("  cache:    %s:%lld  ttl=%lld\n",
           cm_get_string_or(cfg, "cache.host", "?"),
           (long long)cm_get_int_or(cfg, "cache.port", 0),
           (long long)cm_get_int_or(cfg, "cache.ttl",  0));
    printf("  logging:  level=%s  format=%s\n",
           cm_get_string_or(cfg, "logging.level",  "?"),
           cm_get_string_or(cfg, "logging.format", "?"));
    printf("  features: search=%d  analytics=%d  export=%d\n",
           cm_get_bool_or(cfg, "features.search",    0),
           cm_get_bool_or(cfg, "features.analytics", 0),
           cm_get_bool_or(cfg, "features.export",    0));
}

/* Apply ENV config with manual key mapping (env uses UPPER_SNAKE) */
static void merge_env_layer(cm_ctx_t *cfg, cm_ctx_t *env_cfg)
{
    struct { const char *env_key; const char *cfg_key; int is_int; int is_bool; } map[] = {
        { "SERVER_HOST",       "server.host",        0, 0 },
        { "SERVER_PORT",       "server.port",        1, 0 },
        { "SERVER_SSL",        "server.ssl",         0, 1 },
        { "SERVER_WORKERS",    "server.workers",     1, 0 },
        { "DATABASE_HOST",     "database.host",      0, 0 },
        { "DATABASE_NAME",     "database.name",      0, 0 },
        { "DATABASE_POOL",     "database.pool",      1, 0 },
        { "CACHE_HOST",        "cache.host",         0, 0 },
        { "CACHE_TTL",         "cache.ttl",          1, 0 },
        { "LOGGING_LEVEL",     "logging.level",      0, 0 },
        { "FEATURE_ANALYTICS", "features.analytics", 0, 1 },
        { "FEATURE_EXPORT",    "features.export",    0, 1 },
        { NULL, NULL, 0, 0 }
    };

    for (int i = 0; map[i].env_key; i++) {
        if (!cm_has_key(env_cfg, map[i].env_key)) continue;

        if (map[i].is_int) {
            int64_t v = cm_get_int_or(env_cfg, map[i].env_key, 0);
            cm_set_int(cfg, map[i].cfg_key, v);
        } else if (map[i].is_bool) {
            int v = cm_get_bool_or(env_cfg, map[i].env_key, 0);
            cm_set_bool(cfg, map[i].cfg_key, v);
        } else {
            const char *v = cm_get_string_or(env_cfg, map[i].env_key, NULL);
            if (v) cm_set_string(cfg, map[i].cfg_key, v);
        }
    }
}

int main(void)
{
    printf("=== config_merge: layered configuration ===");

    cm_ctx_t *cfg = cm_ctx_create();

    /* Layer 0: defaults */
    apply_defaults(cfg);
    print_layer(cfg, "Layer 0: Defaults");

    /* Layer 1: base JSON config */
    cm_ctx_t *base = cm_ctx_create();
    if (cm_load_string(base, BASE_JSON, CM_FORMAT_JSON) == CM_OK) {
        cm_merge(cfg, base, /*overwrite=*/1);
        print_layer(cfg, "Layer 1: After base JSON");
    } else {
        printf("\n  (JSON not available, skipping layer 1)\n");
    }
    cm_ctx_destroy(base);

    /* Layer 2: production env */
    cm_ctx_t *env = cm_ctx_create();
    if (cm_load_string(env, PROD_ENV, CM_FORMAT_ENV) == CM_OK) {
        merge_env_layer(cfg, env);
        print_layer(cfg, "Layer 2: After .env override");
    } else {
        printf("\n  (ENV parse failed, skipping layer 2)\n");
    }
    cm_ctx_destroy(env);

    /* Layer 3: CLI overrides */
    apply_cli_overrides(cfg);
    print_layer(cfg, "Layer 3: After CLI overrides (FINAL)");

    /* Dump full config as YAML */
    printf("\n--- Final config as YAML ---\n");
    size_t yl = 0;
    char *yaml = cm_save_string(cfg, CM_FORMAT_YAML, &yl);
    if (yaml && yl > 0) { printf("%s\n", yaml); free(yaml); }
    else printf("  (YAML not available)\n");

    cm_ctx_destroy(cfg);
    return 0;
}
