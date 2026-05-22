/**
 * server_config.c  –  Real-world example: layered server configuration.
 *
 * Pattern:
 *   1. Load hard-coded defaults
 *   2. Override from config file (JSON/YAML/TOML/INI auto-detected)
 *   3. Override from .env environment file
 *   4. Read final values and start "server"
 */
#include "config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Typed config struct (application layer) ────────────────────── */

typedef struct {
    char    host[128];
    int     port;
    int     ssl;
    char    cert_file[256];
    char    key_file[256];
    int     workers;
    int     max_connections;
    double  request_timeout;
    char    log_level[16];
    char    log_file[256];
    char    db_host[128];
    int     db_port;
    char    db_name[128];
    int     db_pool;
} server_cfg_t;

static void load_defaults(cm_ctx_t *cfg)
{
    cm_set_string(cfg, "server.host",        "127.0.0.1");
    cm_set_int   (cfg, "server.port",        8080);
    cm_set_bool  (cfg, "server.ssl",         0);
    cm_set_string(cfg, "server.cert_file",   "");
    cm_set_string(cfg, "server.key_file",    "");
    cm_set_int   (cfg, "server.workers",     2);
    cm_set_int   (cfg, "server.max_conn",    100);
    cm_set_float (cfg, "server.req_timeout", 30.0);

    cm_set_string(cfg, "logging.level", "info");
    cm_set_string(cfg, "logging.file",  "stdout");

    cm_set_string(cfg, "database.host",  "localhost");
    cm_set_int   (cfg, "database.port",  5432);
    cm_set_string(cfg, "database.name",  "app");
    cm_set_int   (cfg, "database.pool",  5);
}

static void read_into_struct(cm_ctx_t *cfg, server_cfg_t *s)
{
    const char *v;

    v = cm_get_string_or(cfg, "server.host", "127.0.0.1");
    snprintf(s->host, sizeof(s->host), "%s", v);

    s->port            = (int)cm_get_int_or  (cfg, "server.port",        8080);
    s->ssl             =      cm_get_bool_or  (cfg, "server.ssl",         0);
    s->workers         = (int)cm_get_int_or  (cfg, "server.workers",     2);
    s->max_connections = (int)cm_get_int_or  (cfg, "server.max_conn",    100);
    s->request_timeout =      cm_get_float_or(cfg, "server.req_timeout", 30.0);

    v = cm_get_string_or(cfg, "server.cert_file", "");
    snprintf(s->cert_file, sizeof(s->cert_file), "%s", v);

    v = cm_get_string_or(cfg, "server.key_file", "");
    snprintf(s->key_file, sizeof(s->key_file), "%s", v);

    v = cm_get_string_or(cfg, "logging.level", "info");
    snprintf(s->log_level, sizeof(s->log_level), "%s", v);

    v = cm_get_string_or(cfg, "logging.file", "stdout");
    snprintf(s->log_file, sizeof(s->log_file), "%s", v);

    v = cm_get_string_or(cfg, "database.host", "localhost");
    snprintf(s->db_host, sizeof(s->db_host), "%s", v);

    s->db_port = (int)cm_get_int_or  (cfg, "database.port", 5432);

    v = cm_get_string_or(cfg, "database.name", "app");
    snprintf(s->db_name, sizeof(s->db_name), "%s", v);

    s->db_pool = (int)cm_get_int_or(cfg, "database.pool", 5);
}

static void print_cfg(const server_cfg_t *s)
{
    printf("  Server  : %s:%d  (ssl=%s, workers=%d, max_conn=%d, timeout=%.1fs)\n",
           s->host, s->port, s->ssl?"ON":"OFF",
           s->workers, s->max_connections, s->request_timeout);
    if (s->ssl && s->cert_file[0])
        printf("  TLS     : cert=%s  key=%s\n", s->cert_file, s->key_file);
    printf("  Logging : level=%s  file=%s\n", s->log_level, s->log_file);
    printf("  Database: %s:%d/%s  pool=%d\n",
           s->db_host, s->db_port, s->db_name, s->db_pool);
}

/* ─── In-memory config files for demo ───────────────────────────── */

static const char *JSON_OVERRIDE =
    "{"
    "  \"server\": {"
    "    \"host\": \"0.0.0.0\","
    "    \"port\": 9443,"
    "    \"ssl\":  true,"
    "    \"cert_file\": \"/etc/ssl/certs/app.crt\","
    "    \"key_file\":  \"/etc/ssl/private/app.key\","
    "    \"workers\":   8,"
    "    \"max_conn\":  1000,"
    "    \"req_timeout\": 60.0"
    "  },"
    "  \"logging\": {"
    "    \"level\": \"warn\","
    "    \"file\":  \"/var/log/app.log\""
    "  },"
    "  \"database\": {"
    "    \"host\": \"db.prod.internal\","
    "    \"port\": 5432,"
    "    \"name\": \"production\","
    "    \"pool\": 25"
    "  }"
    "}";

static const char *ENV_OVERRIDE =
    "SERVER_PORT=8080\n"         /* intentionally override back */
    "SERVER_WORKERS=16\n"
    "DATABASE_POOL=30\n"
    "LOGGING_LEVEL=debug\n";

int main(void)
{
    printf("=== server_config: layered config example ===\n\n");

    cm_ctx_t *cfg = cm_ctx_create();

    /* Layer 1: defaults */
    printf("[1] Loading defaults...\n");
    load_defaults(cfg);

    /* Layer 2: JSON config override */
    printf("[2] Applying JSON override...\n");
    cm_ctx_t *json_cfg = cm_ctx_create();
    cm_error_t err = cm_load_string(json_cfg, JSON_OVERRIDE, CM_FORMAT_JSON);
    if (err == CM_OK) {
        cm_merge(cfg, json_cfg, /*overwrite=*/1);
        printf("    JSON override applied.\n");
    } else {
        printf("    JSON not available (%s)\n", cm_error_str(err));
    }
    cm_ctx_destroy(json_cfg);

    /* Layer 3: ENV override (simulate env file, e.g. loaded at runtime) */
    printf("[3] Applying ENV override...\n");
    cm_ctx_t *env_cfg = cm_ctx_create();
    err = cm_load_string(env_cfg, ENV_OVERRIDE, CM_FORMAT_ENV);
    if (err == CM_OK) {
        /* ENV keys are uppercase with underscores; manually apply */
        const char *lv = cm_get_string_or(env_cfg, "LOGGING_LEVEL", NULL);
        if (lv) cm_set_string(cfg, "logging.level", lv);

        int64_t p = cm_get_int_or(env_cfg, "SERVER_PORT", -1);
        if (p > 0) cm_set_int(cfg, "server.port", p);

        int64_t w = cm_get_int_or(env_cfg, "SERVER_WORKERS", -1);
        if (w > 0) cm_set_int(cfg, "server.workers", w);

        int64_t pool = cm_get_int_or(env_cfg, "DATABASE_POOL", -1);
        if (pool > 0) cm_set_int(cfg, "database.pool", (int)pool);

        printf("    ENV override applied.\n");
    }
    cm_ctx_destroy(env_cfg);

    /* Final config */
    printf("\n[Final configuration]\n");
    server_cfg_t s = {0};
    read_into_struct(cfg, &s);
    print_cfg(&s);

    /* Export final config as TOML for audit */
    printf("\n[Exporting final config as TOML]\n");
    size_t toml_len = 0;
    char *toml_str = cm_save_string(cfg, CM_FORMAT_TOML, &toml_len);
    if (toml_str && toml_len > 0) {
        printf("%s\n", toml_str);
        free(toml_str);
    } else {
        printf("  (TOML not available)\n");
    }

    cm_ctx_destroy(cfg);
    return 0;
}
