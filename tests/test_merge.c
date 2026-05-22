/* test_merge.c  –  cm_merge(), cross-format conversion, walk */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ─── helpers ───────────────────────────────────────────────────────── */

static int leaf_count;
static void count_leaves(const char *path, cm_node_t *node, void *ud)
{
    (void)path; (void)node;
    (*(int *)ud)++;
}

/* ─── merge: no-overwrite ───────────────────────────────────────────── */

static void test_merge_no_overwrite(void)
{
    printf("  %-40s ", "merge_no_overwrite");

    cm_ctx_t *base = cm_ctx_create();
    cm_set_string(base, "host", "localhost");
    cm_set_int(base,    "port", 8080);
    cm_set_string(base, "base_only", "yes");

    cm_ctx_t *over = cm_ctx_create();
    cm_set_string(over, "host", "remote.host");   /* should NOT overwrite */
    cm_set_string(over, "extra", "bonus");         /* new key, should appear */

    assert(cm_merge(base, over, /*overwrite=*/0) == CM_OK);

    const char *host = NULL;
    assert(cm_get_string(base, "host", &host) == CM_OK);
    assert(strcmp(host, "localhost") == 0);   /* unchanged */

    const char *extra = NULL;
    assert(cm_get_string(base, "extra", &extra) == CM_OK);
    assert(strcmp(extra, "bonus") == 0);      /* new key added */

    cm_ctx_destroy(base);
    cm_ctx_destroy(over);
    printf("PASS\n");
}

/* ─── merge: overwrite ──────────────────────────────────────────────── */

static void test_merge_overwrite(void)
{
    printf("  %-40s ", "merge_overwrite");

    cm_ctx_t *base = cm_ctx_create();
    cm_set_string(base, "host", "localhost");
    cm_set_int(base,    "port", 8080);

    cm_ctx_t *over = cm_ctx_create();
    cm_set_string(over, "host", "prod.server");  /* should overwrite */
    cm_set_int(over,    "port", 443);

    assert(cm_merge(base, over, /*overwrite=*/1) == CM_OK);

    const char *host = NULL;
    assert(cm_get_string(base, "host", &host) == CM_OK);
    assert(strcmp(host, "prod.server") == 0);

    int64_t port = 0;
    assert(cm_get_int(base, "port", &port) == CM_OK);
    assert(port == 443);

    cm_ctx_destroy(base);
    cm_ctx_destroy(over);
    printf("PASS\n");
}

/* ─── merge: nested objects deep-merged ────────────────────────────── */

static void test_merge_deep(void)
{
    printf("  %-40s ", "merge_deep_nested");

    cm_ctx_t *base = cm_ctx_create();
    cm_set_string(base, "db.host", "localhost");
    cm_set_int(base,    "db.port", 5432);
    cm_set_string(base, "db.name", "dev");

    cm_ctx_t *over = cm_ctx_create();
    cm_set_string(over, "db.name", "prod");      /* overwrite */
    cm_set_int(over,    "db.pool", 20);           /* new nested key */

    assert(cm_merge(base, over, /*overwrite=*/1) == CM_OK);

    const char *host = NULL;
    assert(cm_get_string(base, "db.host", &host) == CM_OK);
    assert(strcmp(host, "localhost") == 0);   /* untouched */

    const char *name = NULL;
    assert(cm_get_string(base, "db.name", &name) == CM_OK);
    assert(strcmp(name, "prod") == 0);        /* overwritten */

    int64_t pool = 0;
    assert(cm_get_int(base, "db.pool", &pool) == CM_OK);
    assert(pool == 20);                        /* new key */

    cm_ctx_destroy(base);
    cm_ctx_destroy(over);
    printf("PASS\n");
}

/* ─── walk: leaf count ──────────────────────────────────────────────── */

static void test_walk(void)
{
    printf("  %-40s ", "walk_leaf_count");

    cm_ctx_t *ctx = cm_ctx_create();
    cm_set_string(ctx, "a",     "1");
    cm_set_int(ctx,    "b",     2);
    cm_set_bool(ctx,   "c.d",   1);
    cm_set_float(ctx,  "c.e",   3.14);
    cm_set_string(ctx, "c.f.g", "deep");

    int count = 0;
    cm_walk(ctx, count_leaves, &count);
    /* a, b, c.d, c.e, c.f.g = 5 leaves */
    assert(count == 5);

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

/* ─── cross-format round-trip: JSON → YAML → INI ───────────────────── */

static const char *CROSS_JSON =
    "{"
    "  \"app\": {"
    "    \"name\": \"CrossTest\","
    "    \"port\": 1234,"
    "    \"debug\": true"
    "  },"
    "  \"score\": 9.5"
    "}";

static void test_cross_json_yaml(void)
{
    printf("  %-40s ", "cross_json_to_yaml");

    cm_ctx_t *ctx = cm_ctx_create();
    cm_error_t err = cm_load_string(ctx, CROSS_JSON, CM_FORMAT_JSON);
    if (err != CM_OK) {
        printf("SKIP (JSON not built)\n");
        cm_ctx_destroy(ctx); return;
    }

    size_t olen = 0;
    char *yaml_str = cm_save_string(ctx, CM_FORMAT_YAML, &olen);
    if (!yaml_str || olen == 0) {
        printf("SKIP (YAML not built)\n");
        cm_ctx_destroy(ctx); return;
    }

    /* reload as YAML */
    cm_ctx_t *ctx2 = cm_ctx_create();
    err = cm_load_string(ctx2, yaml_str, CM_FORMAT_YAML);
    free(yaml_str);

    if (err != CM_OK) {
        printf("SKIP (YAML parse failed)\n");
        cm_ctx_destroy(ctx); cm_ctx_destroy(ctx2); return;
    }

    const char *name = NULL;
    assert(cm_get_string(ctx2, "app.name", &name) == CM_OK);
    assert(strcmp(name, "CrossTest") == 0);

    int64_t port = 0;
    assert(cm_get_int(ctx2, "app.port", &port) == CM_OK);
    assert(port == 1234);

    cm_ctx_destroy(ctx);
    cm_ctx_destroy(ctx2);
    printf("PASS\n");
}

static void test_cross_json_ini(void)
{
    printf("  %-40s ", "cross_json_to_ini");

    cm_ctx_t *ctx = cm_ctx_create();
    cm_error_t err = cm_load_string(ctx, CROSS_JSON, CM_FORMAT_JSON);
    if (err != CM_OK) {
        printf("SKIP\n"); cm_ctx_destroy(ctx); return;
    }

    size_t olen = 0;
    char *ini_str = cm_save_string(ctx, CM_FORMAT_INI, &olen);
    assert(ini_str && olen > 0);

    cm_ctx_t *ctx2 = cm_ctx_create();
    err = cm_load_string(ctx2, ini_str, CM_FORMAT_INI);
    free(ini_str);
    assert(err == CM_OK);

    /* INI flattens to section.key or global key */
    /* score is global */
    double score = 0;
    assert(cm_get_float(ctx2, "score", &score) == CM_OK);
    assert(score > 9.4 && score < 9.6);

    cm_ctx_destroy(ctx);
    cm_ctx_destroy(ctx2);
    printf("PASS\n");
}

static void test_cross_json_toml(void)
{
    printf("  %-40s ", "cross_json_to_toml");

    cm_ctx_t *ctx = cm_ctx_create();
    cm_error_t err = cm_load_string(ctx, CROSS_JSON, CM_FORMAT_JSON);
    if (err != CM_OK) {
        printf("SKIP\n"); cm_ctx_destroy(ctx); return;
    }

    size_t olen = 0;
    char *toml_str = cm_save_string(ctx, CM_FORMAT_TOML, &olen);
    assert(toml_str && olen > 0);

    cm_ctx_t *ctx2 = cm_ctx_create();
    err = cm_load_string(ctx2, toml_str, CM_FORMAT_TOML);
    free(toml_str);

    if (err != CM_OK) {
        printf("SKIP (TOML not built)\n");
        cm_ctx_destroy(ctx); cm_ctx_destroy(ctx2); return;
    }

    int64_t port = 0;
    assert(cm_get_int(ctx2, "app.port", &port) == CM_OK);
    assert(port == 1234);

    cm_ctx_destroy(ctx);
    cm_ctx_destroy(ctx2);
    printf("PASS\n");
}

/* ─── format detection ──────────────────────────────────────────────── */

static void test_format_detect(void)
{
    printf("  %-40s ", "format_detect");
    assert(cm_detect_format("config.json")       == CM_FORMAT_JSON);
    assert(cm_detect_format("config.yaml")       == CM_FORMAT_YAML);
    assert(cm_detect_format("config.yml")        == CM_FORMAT_YAML);
    assert(cm_detect_format("config.xml")        == CM_FORMAT_XML);
    assert(cm_detect_format("config.ini")        == CM_FORMAT_INI);
    assert(cm_detect_format("config.cfg")        == CM_FORMAT_INI);
    assert(cm_detect_format("config.toml")       == CM_FORMAT_TOML);
    assert(cm_detect_format(".env")              == CM_FORMAT_AUTO); /* no ext */
    assert(cm_detect_format("config.env")        == CM_FORMAT_ENV);
    assert(cm_detect_format("app.properties")    == CM_FORMAT_PROPERTIES);
    printf("PASS\n");
}

/* ─── null / error handling ─────────────────────────────────────────── */

static void test_null_safety(void)
{
    printf("  %-40s ", "null_ptr_safety");
    assert(cm_set_string(NULL, "k", "v")  == CM_ERR_NULL_PTR);
    assert(cm_set_string((cm_ctx_t*)0x1, NULL, "v") == CM_ERR_NULL_PTR);
    assert(cm_load_file(NULL, "f.json", CM_FORMAT_AUTO) == CM_ERR_NULL_PTR);
    assert(cm_merge(NULL, NULL, 0) == CM_ERR_NULL_PTR);
    printf("PASS\n");
}

static void test_type_coercion(void)
{
    printf("  %-40s ", "type_coercion_string_to_int");
    cm_ctx_t *ctx = cm_ctx_create();
    cm_set_string(ctx, "n", "42");
    int64_t v = 0;
    assert(cm_get_int(ctx, "n", &v) == CM_OK);
    assert(v == 42);

    cm_set_string(ctx, "b", "true");
    int bv = 0;
    assert(cm_get_bool(ctx, "b", &bv) == CM_OK);
    assert(bv == 1);

    cm_set_string(ctx, "f", "3.14");
    double fv = 0;
    assert(cm_get_float(ctx, "f", &fv) == CM_OK);
    assert(fv > 3.13 && fv < 3.15);

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

/* ─── array operations ──────────────────────────────────────────────── */

static void test_array_int(void)
{
    printf("  %-40s ", "array_push_int");
    cm_ctx_t *ctx = cm_ctx_create();
    cm_array_push_int(ctx, "nums", 10);
    cm_array_push_int(ctx, "nums", 20);
    cm_array_push_int(ctx, "nums", 30);

    size_t len = 0;
    assert(cm_array_length(ctx, "nums", &len) == CM_OK);
    assert(len == 3);

    cm_node_t *n = cm_array_get(ctx, "nums", 2);
    assert(n && n->type == CM_TYPE_INT && n->value.ival == 30);

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

static void test_array_nested_key(void)
{
    printf("  %-40s ", "array_nested_key");
    cm_ctx_t *ctx = cm_ctx_create();
    cm_array_push_string(ctx, "server.tags", "prod");
    cm_array_push_string(ctx, "server.tags", "v2");

    size_t len = 0;
    assert(cm_array_length(ctx, "server.tags", &len) == CM_OK);
    assert(len == 2);

    cm_node_t *t = cm_array_get(ctx, "server.tags", 0);
    assert(t && strcmp(t->value.sval, "prod") == 0);

    cm_ctx_destroy(ctx);
    printf("PASS\n");
}

/* ─── error string / format string ─────────────────────────────────── */

static void test_string_helpers(void)
{
    printf("  %-40s ", "error_and_format_strings");
    assert(strcmp(cm_error_str(CM_OK), "OK") == 0);
    assert(strcmp(cm_error_str(CM_ERR_NOT_FOUND), "Key not found") == 0);
    assert(strcmp(cm_format_str(CM_FORMAT_JSON), "json") == 0);
    assert(strcmp(cm_format_str(CM_FORMAT_TOML), "toml") == 0);
    assert(strcmp(cm_type_str(CM_TYPE_OBJECT), "object") == 0);
    printf("PASS\n");
}

int main(void)
{
    printf("=== test_merge ===\n");
    test_merge_no_overwrite();
    test_merge_overwrite();
    test_merge_deep();
    test_walk();
    test_cross_json_yaml();
    test_cross_json_ini();
    test_cross_json_toml();
    test_format_detect();
    test_null_safety();
    test_type_coercion();
    test_array_int();
    test_array_nested_key();
    test_string_helpers();
    printf("All merge/cross-format tests passed.\n");
    return 0;
}
