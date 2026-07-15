/* test_env.c  –  tests for .env and .properties formats */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ─── .env ─────────────────────────────────────────────────────────── */

static const char *ENV_DATA =
    "# Application environment\n"
    "APP_NAME=MyService\n"
    "APP_ENV=production\n"
    "APP_PORT=8080\n"
    "APP_DEBUG=false\n"
    "APP_WORKERS=8\n"
    "APP_RATIO=0.75\n"
    "DATABASE_URL=\"postgres://user:pass@localhost:5432/db\"\n"
    "export SECRET_KEY=supersecret123\n"
    "ALLOWED_HOSTS=\"localhost 127.0.0.1\"\n"
    "FEATURE_SEARCH=true\n";

static void test_env_load(void)
{
    printf("  %-40s ", "env_load_string");
    cm_ctx_t *ctx = cm_ctx_create();

    assert(cm_load_string(ctx, ENV_DATA, CM_FORMAT_ENV) == CM_OK);

    const char *name = NULL;
    assert(cm_get_string(ctx, "APP_NAME", &name) == CM_OK);
    assert(strcmp(name, "MyService") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx, "APP_PORT", &port) == CM_OK);
    assert(port == 8080);

    int debug = 1;
    assert(cm_get_bool(ctx, "APP_DEBUG", &debug) == CM_OK);
    assert(debug == 0);

    int64_t workers = 0;
    assert(cm_get_int(ctx, "APP_WORKERS", &workers) == CM_OK);
    assert(workers == 8);

    double ratio = 0;
    assert(cm_get_float(ctx, "APP_RATIO", &ratio) == CM_OK);
    assert(ratio > 0.74 && ratio < 0.76);

    const char *secret = NULL;
    assert(cm_get_string(ctx, "SECRET_KEY", &secret) == CM_OK);
    assert(strcmp(secret, "supersecret123") == 0);

    /* quoted URL should be stripped of quotes */
    const char *db_url = NULL;
    assert(cm_get_string(ctx, "DATABASE_URL", &db_url) == CM_OK);
    assert(strstr(db_url, "postgres://") != NULL);

    int feat = 0;
    assert(cm_get_bool(ctx, "FEATURE_SEARCH", &feat) == CM_OK);
    assert(feat == 1);

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

static void test_env_roundtrip(void)
{
    printf("  %-40s ", "env_roundtrip");
    cm_ctx_t *ctx = cm_ctx_create();
    cm_load_string(ctx, ENV_DATA, CM_FORMAT_ENV);

    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_ENV, &olen);
    assert(out && olen > 0);

    /* reload */
    cm_ctx_t *ctx2 = cm_ctx_create();
    assert(cm_load_string(ctx2, out, CM_FORMAT_ENV) == CM_OK);
    free(out);

    int64_t port = 0;
    assert(cm_get_int(ctx2, "APP_PORT", &port) == CM_OK);
    assert(port == 8080);

    cm_ctx_destroy(ctx2);
    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

/* ─── .properties ───────────────────────────────────────────────────── */

static const char *PROPS_DATA =
    "# Java-style properties file\n"
    "app.name = AwesomeApp\n"
    "app.version = 3.1.4\n"
    "app.debug = false\n"
    "server.host = 192.168.1.10\n"
    "server.port = 9000\n"
    "server.ssl  = true\n"
    "db.host: db.internal\n"
    "db.port: 3306\n"
    "db.name: warehouse\n"
    "db.pool  10\n"
    "log.level = warn\n"
    "log.file  = /var/log/app.log\n";

static void test_properties_load(void)
{
    printf("  %-40s ", "properties_load_string");
    cm_ctx_t *ctx = cm_ctx_create();

    assert(cm_load_string(ctx, PROPS_DATA, CM_FORMAT_PROPERTIES) == CM_OK);

    const char *app_name = NULL;
    assert(cm_get_string(ctx, "app.name", &app_name) == CM_OK);
    assert(strcmp(app_name, "AwesomeApp") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx, "server.port", &port) == CM_OK);
    assert(port == 9000);

    int ssl = 0;
    assert(cm_get_bool(ctx, "server.ssl", &ssl) == CM_OK);
    assert(ssl == 1);

    const char *log_level = NULL;
    assert(cm_get_string(ctx, "log.level", &log_level) == CM_OK);
    assert(strcmp(log_level, "warn") == 0);

    /* colon separator */
    const char *db_host = NULL;
    assert(cm_get_string(ctx, "db.host", &db_host) == CM_OK);
    assert(strcmp(db_host, "db.internal") == 0);

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

static void test_properties_roundtrip(void)
{
    printf("  %-40s ", "properties_roundtrip");
    cm_ctx_t *ctx = cm_ctx_create();
    cm_load_string(ctx, PROPS_DATA, CM_FORMAT_PROPERTIES);

    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_PROPERTIES, &olen);
    assert(out && olen > 0);

    cm_ctx_t *ctx2 = cm_ctx_create();
    assert(cm_load_string(ctx2, out, CM_FORMAT_PROPERTIES) == CM_OK);
    free(out);

    const char *name = NULL;
    assert(cm_get_string(ctx2, "app.name", &name) == CM_OK);
    assert(strcmp(name, "AwesomeApp") == 0);

    cm_ctx_destroy(ctx2);
    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

/* ─── edge cases ────────────────────────────────────────────────────── */

static void test_env_empty_value(void)
{
    printf("  %-40s ", "env_empty_value");
    cm_ctx_t *ctx = cm_ctx_create();
    assert(cm_load_string(ctx, "EMPTY=\nNORMAL=ok\n", CM_FORMAT_ENV) == CM_OK);

    const char *e = NULL;
    /* EMPTY key may or may not be present; NORMAL must be */
    const char *n = NULL;
    assert(cm_get_string(ctx, "NORMAL", &n) == CM_OK);
    assert(strcmp(n, "ok") == 0);
    (void)e;

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

static void test_env_comment_lines(void)
{
    printf("  %-40s ", "env_comment_lines");
    cm_ctx_t *ctx = cm_ctx_create();
    const char *data =
        "# this is a comment\n"
        "KEY=value\n"
        "   # indented comment\n"
        "OTHER=42\n";
    assert(cm_load_string(ctx, data, CM_FORMAT_ENV) == CM_OK);
    assert(!cm_has_key(ctx, "#"));
    const char *v = NULL;
    assert(cm_get_string(ctx, "KEY", &v) == CM_OK);
    assert(strcmp(v, "value") == 0);
    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

static void test_env_export_prefix(void)
{
    printf("  %-40s ", "env_export_prefix");
    cm_ctx_t *ctx = cm_ctx_create();
    assert(cm_load_string(ctx, "export MY_VAR=hello\n", CM_FORMAT_ENV) == CM_OK);
    const char *v = NULL;
    assert(cm_get_string(ctx, "MY_VAR", &v) == CM_OK);
    assert(strcmp(v, "hello") == 0);
    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

static void test_properties_blank_lines(void)
{
    printf("  %-40s ", "properties_blank_lines");
    cm_ctx_t *ctx = cm_ctx_create();
    assert(cm_load_string(ctx, "\n   \nkey=value\n", CM_FORMAT_PROPERTIES) == CM_OK);
    const char *value = NULL;
    assert(cm_get_string(ctx, "key", &value) == CM_OK);
    assert(strcmp(value, "value") == 0);
    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

int main(void)
{
    printf("=== test_env ===\n");
    test_env_load();
    test_env_roundtrip();
    test_properties_load();
    test_properties_roundtrip();
    test_env_empty_value();
    test_env_comment_lines();
    test_env_export_prefix();
    test_properties_blank_lines();
    printf("All ENV/Properties tests passed.\n");
    return 0;
}
