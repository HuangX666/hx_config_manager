#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

/**
 * config_manager.h
 * Unified configuration management library (Pure C, multi-platform)
 * Supports: JSON, YAML, XML, INI, TOML, ENV, Properties
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────── */
#define CM_VERSION_MAJOR 1
#define CM_VERSION_MINOR 0
#define CM_VERSION_PATCH 0
#define CM_VERSION_STR   "1.0.0"

/* ─────────────────────────────────────────────────
 * Error codes
 * ───────────────────────────────────────────────── */
typedef enum {
    CM_OK               =  0,
    CM_ERR_NULL_PTR     = -1,
    CM_ERR_NO_MEMORY    = -2,
    CM_ERR_NOT_FOUND    = -3,
    CM_ERR_TYPE_MISMATCH= -4,
    CM_ERR_PARSE        = -5,
    CM_ERR_IO           = -6,
    CM_ERR_UNSUPPORTED  = -7,
    CM_ERR_INVALID_KEY  = -8,
    CM_ERR_OVERFLOW     = -9,
    CM_ERR_DUPLICATE    = -10,
} cm_error_t;

/* ─────────────────────────────────────────────────
 * Supported formats
 * ───────────────────────────────────────────────── */
typedef enum {
    CM_FORMAT_AUTO       = 0,  /* detect from file extension */
    CM_FORMAT_JSON       = 1,
    CM_FORMAT_YAML       = 2,
    CM_FORMAT_XML        = 3,
    CM_FORMAT_INI        = 4,
    CM_FORMAT_TOML       = 5,
    CM_FORMAT_ENV        = 6,
    CM_FORMAT_PROPERTIES = 7,
} cm_format_t;

/* ─────────────────────────────────────────────────
 * Value types
 * ───────────────────────────────────────────────── */
typedef enum {
    CM_TYPE_NULL    = 0,
    CM_TYPE_BOOL    = 1,
    CM_TYPE_INT     = 2,
    CM_TYPE_FLOAT   = 3,
    CM_TYPE_STRING  = 4,
    CM_TYPE_ARRAY   = 5,
    CM_TYPE_OBJECT  = 6,
} cm_type_t;

/* ─────────────────────────────────────────────────
 * Config node (tree node)
 * ───────────────────────────────────────────────── */
typedef struct cm_node cm_node_t;

struct cm_node {
    char         *key;
    cm_type_t     type;

    union {
        int64_t   ival;
        double    fval;
        int       bval;
        char     *sval;
        struct {
            cm_node_t **items;
            size_t      count;
            size_t      capacity;
        } array;
        struct {
            cm_node_t **children;
            size_t      count;
            size_t      capacity;
        } object;
    } value;

    cm_node_t *parent;
    cm_node_t *next;   /* sibling linked list */
};

/* ─────────────────────────────────────────────────
 * Config context
 * ───────────────────────────────────────────────── */
typedef struct {
    cm_node_t   *root;
    cm_format_t  format;
    char        *source_path;   /* last loaded file path */
    int          dirty;         /* modified since last save */
} cm_ctx_t;

/* ─────────────────────────────────────────────────
 * Context lifecycle
 * ───────────────────────────────────────────────── */
cm_ctx_t   *cm_ctx_create(void);
void        cm_ctx_destroy(cm_ctx_t *ctx);
void        cm_ctx_clear(cm_ctx_t *ctx);

/* ─────────────────────────────────────────────────
 * Load / Save
 * ───────────────────────────────────────────────── */
cm_error_t  cm_load_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt);
cm_error_t  cm_load_string(cm_ctx_t *ctx, const char *data, cm_format_t fmt);
cm_error_t  cm_save_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt);
char       *cm_save_string(cm_ctx_t *ctx, cm_format_t fmt, size_t *out_len);

/* ─────────────────────────────────────────────────
 * Key access  (dot-separated path: "server.host")
 * For arrays use:  "list[0].name"
 * ───────────────────────────────────────────────── */
cm_node_t  *cm_get_node(cm_ctx_t *ctx, const char *key_path);
cm_error_t  cm_get_string(cm_ctx_t *ctx, const char *key, const char **out);
cm_error_t  cm_get_int(cm_ctx_t *ctx, const char *key, int64_t *out);
cm_error_t  cm_get_float(cm_ctx_t *ctx, const char *key, double *out);
cm_error_t  cm_get_bool(cm_ctx_t *ctx, const char *key, int *out);

/* With default fallback */
const char *cm_get_string_or(cm_ctx_t *ctx, const char *key, const char *def);
int64_t     cm_get_int_or(cm_ctx_t *ctx, const char *key, int64_t def);
double      cm_get_float_or(cm_ctx_t *ctx, const char *key, double def);
int         cm_get_bool_or(cm_ctx_t *ctx, const char *key, int def);

/* ─────────────────────────────────────────────────
 * Key mutation
 * ───────────────────────────────────────────────── */
cm_error_t  cm_set_string(cm_ctx_t *ctx, const char *key, const char *val);
cm_error_t  cm_set_int(cm_ctx_t *ctx, const char *key, int64_t val);
cm_error_t  cm_set_float(cm_ctx_t *ctx, const char *key, double val);
cm_error_t  cm_set_bool(cm_ctx_t *ctx, const char *key, int val);
cm_error_t  cm_set_null(cm_ctx_t *ctx, const char *key);
cm_error_t  cm_delete(cm_ctx_t *ctx, const char *key);
int         cm_has_key(cm_ctx_t *ctx, const char *key);

/* ─────────────────────────────────────────────────
 * Array helpers
 * ───────────────────────────────────────────────── */
cm_error_t  cm_array_length(cm_ctx_t *ctx, const char *key, size_t *out);
cm_node_t  *cm_array_get(cm_ctx_t *ctx, const char *key, size_t index);
cm_error_t  cm_array_push_string(cm_ctx_t *ctx, const char *key, const char *val);
cm_error_t  cm_array_push_int(cm_ctx_t *ctx, const char *key, int64_t val);

/* ─────────────────────────────────────────────────
 * Merge & iterate
 * ───────────────────────────────────────────────── */
cm_error_t  cm_merge(cm_ctx_t *dst, const cm_ctx_t *src, int overwrite);

typedef void (*cm_walk_fn)(const char *path, cm_node_t *node, void *userdata);
void        cm_walk(cm_ctx_t *ctx, cm_walk_fn fn, void *userdata);

/* ─────────────────────────────────────────────────
 * Utilities
 * ───────────────────────────────────────────────── */
const char *cm_error_str(cm_error_t err);
const char *cm_type_str(cm_type_t t);
const char *cm_format_str(cm_format_t f);
cm_format_t cm_detect_format(const char *path);
void        cm_dump(cm_ctx_t *ctx);  /* debug print */

/* Node helpers (for manual tree building) */
cm_node_t  *cm_node_new_string(const char *key, const char *val);
cm_node_t  *cm_node_new_int(const char *key, int64_t val);
cm_node_t  *cm_node_new_float(const char *key, double val);
cm_node_t  *cm_node_new_bool(const char *key, int val);
cm_node_t  *cm_node_new_null(const char *key);
cm_node_t  *cm_node_new_object(const char *key);
cm_node_t  *cm_node_new_array(const char *key);
void        cm_node_free(cm_node_t *node);
cm_error_t  cm_node_object_add(cm_node_t *obj, cm_node_t *child);
cm_error_t  cm_node_array_add(cm_node_t *arr, cm_node_t *item);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_MANAGER_H */
