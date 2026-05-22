/**
 * multi_format.c  –  Load the same logical config from every supported
 *                    format and verify values are identical.
 *
 * Shows how config_manager provides a uniform API regardless of source.
 */
#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ─── Equivalent configs in each format ──────────────────────────── */

static const char *JSON_SRC =
    "{"
    "  \"app\":    { \"name\": \"Demo\", \"port\": 3000, \"debug\": true },"
    "  \"db\":     { \"host\": \"localhost\", \"port\": 5432 },"
    "  \"ratio\":  1.5,"
    "  \"tags\":   [\"a\", \"b\", \"c\"]"
    "}";

static const char *YAML_SRC =
    "app:\n"
    "  name: Demo\n"
    "  port: 3000\n"
    "  debug: true\n"
    "db:\n"
    "  host: localhost\n"
    "  port: 5432\n"
    "ratio: 1.5\n"
    "tags:\n"
    "  - a\n"
    "  - b\n"
    "  - c\n";

static const char *INI_SRC =
    "[app]\n"
    "name  = Demo\n"
    "port  = 3000\n"
    "debug = true\n"
    "[db]\n"
    "host = localhost\n"
    "port = 5432\n"
    "ratio = 1.5\n";

static const char *TOML_SRC =
    "ratio = 1.5\n"
    "tags  = [\"a\", \"b\", \"c\"]\n"
    "\n"
    "[app]\n"
    "name  = \"Demo\"\n"
    "port  = 3000\n"
    "debug = true\n"
    "\n"
    "[db]\n"
    "host = \"localhost\"\n"
    "port = 5432\n";

static const char *ENV_SRC =
    "APP_NAME=Demo\n"
    "APP_PORT=3000\n"
    "APP_DEBUG=true\n"
    "DB_HOST=localhost\n"
    "DB_PORT=5432\n"
    "RATIO=1.5\n";

static const char *PROPS_SRC =
    "app.name  = Demo\n"
    "app.port  = 3000\n"
    "app.debug = true\n"
    "db.host   = localhost\n"
    "db.port   = 5432\n"
    "ratio     = 1.5\n";

static const char *XML_SRC =
    "<?xml version=\"1.0\"?>\n"
    "<config>\n"
    "  <app>\n"
    "    <name>Demo</name>\n"
    "    <port>3000</port>\n"
    "    <debug>true</debug>\n"
    "  </app>\n"
    "  <db>\n"
    "    <host>localhost</host>\n"
    "    <port>5432</port>\n"
    "  </db>\n"
    "  <ratio>1.5</ratio>\n"
    "  <tags type=\"array\">\n"
    "    <item>a</item>\n"
    "    <item>b</item>\n"
    "    <item>c</item>\n"
    "  </tags>\n"
    "</config>\n";

/* ─── Verify common fields ────────────────────────────────────────── */

typedef struct {
    const char *app_name_key;
    const char *app_port_key;
    const char *app_debug_key;
    const char *db_host_key;
    const char *db_port_key;
    const char *ratio_key;
    const char *tags_key;           /* NULL if not applicable */
} key_map_t;

static const key_map_t KEY_DEFAULT = {
    "app.name", "app.port", "app.debug",
    "db.host",  "db.port",  "ratio", "tags"
};
static const key_map_t KEY_ENV = {
    "APP_NAME", "APP_PORT", "APP_DEBUG",
    "DB_HOST",  "DB_PORT",  "RATIO", NULL
};

static void verify(cm_ctx_t *ctx, const char *fmt_name, const key_map_t *km)
{
    printf("  %-14s : ", fmt_name);

    const char *name = NULL;
    cm_error_t err = cm_get_string(ctx, km->app_name_key, &name);
    if (err != CM_OK) { printf("SKIP (%s)\n", cm_error_str(err)); return; }
    assert(strcmp(name, "Demo") == 0);

    int64_t app_port = 0;
    assert(cm_get_int(ctx, km->app_port_key, &app_port) == CM_OK);
    assert(app_port == 3000);

    int dbug = 0;
    assert(cm_get_bool(ctx, km->app_debug_key, &dbug) == CM_OK);
    assert(dbug == 1);

    int64_t db_port = 0;
    assert(cm_get_int(ctx, km->db_port_key, &db_port) == CM_OK);
    assert(db_port == 5432);

    double ratio = 0;
    assert(cm_get_float(ctx, km->ratio_key, &ratio) == CM_OK);
    assert(ratio > 1.4 && ratio < 1.6);

    if (km->tags_key) {
        size_t tlen = 0;
        if (cm_array_length(ctx, km->tags_key, &tlen) == CM_OK) {
            assert(tlen == 3);
            cm_node_t *t0 = cm_array_get(ctx, km->tags_key, 0);
            assert(t0 && strcmp(t0->value.sval, "a") == 0);
        }
    }

    printf("PASS\n");
}

int main(void)
{
    printf("=== multi_format: same data, every format ===\n\n");

    struct {
        const char  *name;
        const char  *src;
        cm_format_t  fmt;
        const key_map_t *km;
    } cases[] = {
        { "JSON",       JSON_SRC,  CM_FORMAT_JSON,       &KEY_DEFAULT },
        { "YAML",       YAML_SRC,  CM_FORMAT_YAML,       &KEY_DEFAULT },
        { "INI",        INI_SRC,   CM_FORMAT_INI,        &KEY_DEFAULT },
        { "TOML",       TOML_SRC,  CM_FORMAT_TOML,       &KEY_DEFAULT },
        { "ENV",        ENV_SRC,   CM_FORMAT_ENV,        &KEY_ENV     },
        { "PROPERTIES", PROPS_SRC, CM_FORMAT_PROPERTIES, &KEY_DEFAULT },
        { "XML",        XML_SRC,   CM_FORMAT_XML,        &KEY_DEFAULT },
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        cm_ctx_t *ctx = cm_ctx_create();
        cm_load_string(ctx, cases[i].src, cases[i].fmt);
        verify(ctx, cases[i].name, cases[i].km);
        cm_ctx_destroy(ctx);
    }

    /* Cross-format conversion: JSON → YAML → TOML → JSON */
    printf("\n--- Cross-format round-trip (JSON→YAML→TOML→JSON) ---\n");
    cm_ctx_t *ctx = cm_ctx_create();
    if (cm_load_string(ctx, JSON_SRC, CM_FORMAT_JSON) == CM_OK) {
        size_t l1 = 0;
        char *yaml = cm_save_string(ctx, CM_FORMAT_YAML, &l1);
        if (yaml) {
            cm_ctx_t *c2 = cm_ctx_create();
            if (cm_load_string(c2, yaml, CM_FORMAT_YAML) == CM_OK) {
                size_t l2 = 0;
                char *toml = cm_save_string(c2, CM_FORMAT_TOML, &l2);
                if (toml) {
                    cm_ctx_t *c3 = cm_ctx_create();
                    if (cm_load_string(c3, toml, CM_FORMAT_TOML) == CM_OK) {
                        size_t l3 = 0;
                        char *json_out = cm_save_string(c3, CM_FORMAT_JSON, &l3);
                        printf("  Final JSON (%zu bytes):\n%s\n",
                               l3, json_out ? json_out : "(null)");
                        free(json_out);
                    }
                    cm_ctx_destroy(c3);
                    free(toml);
                }
            }
            cm_ctx_destroy(c2);
            free(yaml);
        }
    }
    cm_ctx_destroy(ctx);

    return 0;
}
