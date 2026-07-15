/**
 * cm_core.c  –  Context lifecycle, node tree, key-path resolution,
 *               get/set/delete/walk/merge public API.
 */

#include "config_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════════════════════════════ */

char *cm_internal_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *p = (char *)malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * Node allocation / free
 * ═══════════════════════════════════════════════════════════════════ */

static cm_node_t *node_alloc(void)
{
    cm_node_t *n = (cm_node_t *)calloc(1, sizeof(cm_node_t));
    return n;
}

cm_node_t *cm_node_new_string(const char *key, const char *val)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key        = cm_internal_strdup(key);
    n->type       = CM_TYPE_STRING;
    n->value.sval = cm_internal_strdup(val ? val : "");
    if ((key && !n->key) || !n->value.sval) {
        cm_node_free(n);
        return NULL;
    }
    return n;
}

cm_node_t *cm_node_new_int(const char *key, int64_t val)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key        = cm_internal_strdup(key);
    if (key && !n->key) { cm_node_free(n); return NULL; }
    n->type       = CM_TYPE_INT;
    n->value.ival = val;
    return n;
}

cm_node_t *cm_node_new_float(const char *key, double val)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key        = cm_internal_strdup(key);
    if (key && !n->key) { cm_node_free(n); return NULL; }
    n->type       = CM_TYPE_FLOAT;
    n->value.fval = val;
    return n;
}

cm_node_t *cm_node_new_bool(const char *key, int val)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key        = cm_internal_strdup(key);
    if (key && !n->key) { cm_node_free(n); return NULL; }
    n->type       = CM_TYPE_BOOL;
    n->value.bval = val ? 1 : 0;
    return n;
}

cm_node_t *cm_node_new_null(const char *key)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key  = cm_internal_strdup(key);
    if (key && !n->key) { cm_node_free(n); return NULL; }
    n->type = CM_TYPE_NULL;
    return n;
}

cm_node_t *cm_node_new_object(const char *key)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key  = cm_internal_strdup(key);
    if (key && !n->key) { cm_node_free(n); return NULL; }
    n->type = CM_TYPE_OBJECT;
    return n;
}

cm_node_t *cm_node_new_array(const char *key)
{
    cm_node_t *n = node_alloc();
    if (!n) return NULL;
    n->key  = cm_internal_strdup(key);
    if (key && !n->key) { cm_node_free(n); return NULL; }
    n->type = CM_TYPE_ARRAY;
    return n;
}

void cm_node_free(cm_node_t *n)
{
    if (!n) return;
    free(n->key);
    if (n->type == CM_TYPE_STRING) {
        free(n->value.sval);
    } else if (n->type == CM_TYPE_OBJECT) {
        for (size_t i = 0; i < n->value.object.count; i++)
            cm_node_free(n->value.object.children[i]);
        free(n->value.object.children);
    } else if (n->type == CM_TYPE_ARRAY) {
        for (size_t i = 0; i < n->value.array.count; i++)
            cm_node_free(n->value.array.items[i]);
        free(n->value.array.items);
    }
    free(n);
}

cm_error_t cm_node_object_add(cm_node_t *obj, cm_node_t *child)
{
    if (!obj || !child) return CM_ERR_NULL_PTR;
    if (obj->type != CM_TYPE_OBJECT) return CM_ERR_TYPE_MISMATCH;

    if (obj->value.object.count >= obj->value.object.capacity) {
        if (obj->value.object.capacity > SIZE_MAX / 2)
            return CM_ERR_OVERFLOW;
        size_t newcap = obj->value.object.capacity ? obj->value.object.capacity * 2 : 8;
        if (newcap > SIZE_MAX / sizeof(cm_node_t *))
            return CM_ERR_OVERFLOW;
        cm_node_t **arr = (cm_node_t **)realloc(
            obj->value.object.children, newcap * sizeof(cm_node_t *));
        if (!arr) return CM_ERR_NO_MEMORY;
        obj->value.object.children  = arr;
        obj->value.object.capacity  = newcap;
    }
    obj->value.object.children[obj->value.object.count++] = child;
    child->parent = obj;
    return CM_OK;
}

cm_error_t cm_node_array_add(cm_node_t *arr, cm_node_t *item)
{
    if (!arr || !item) return CM_ERR_NULL_PTR;
    if (arr->type != CM_TYPE_ARRAY) return CM_ERR_TYPE_MISMATCH;

    if (arr->value.array.count >= arr->value.array.capacity) {
        if (arr->value.array.capacity > SIZE_MAX / 2)
            return CM_ERR_OVERFLOW;
        size_t newcap = arr->value.array.capacity ? arr->value.array.capacity * 2 : 8;
        if (newcap > SIZE_MAX / sizeof(cm_node_t *))
            return CM_ERR_OVERFLOW;
        cm_node_t **p = (cm_node_t **)realloc(
            arr->value.array.items, newcap * sizeof(cm_node_t *));
        if (!p) return CM_ERR_NO_MEMORY;
        arr->value.array.items    = p;
        arr->value.array.capacity = newcap;
    }
    arr->value.array.items[arr->value.array.count++] = item;
    item->parent = arr;
    return CM_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * Context lifecycle
 * ═══════════════════════════════════════════════════════════════════ */

cm_ctx_t *cm_ctx_create(void)
{
    cm_ctx_t *ctx = (cm_ctx_t *)calloc(1, sizeof(cm_ctx_t));
    if (!ctx) return NULL;
    ctx->root = cm_node_new_object(NULL);
    if (!ctx->root) { free(ctx); return NULL; }
    return ctx;
}

void cm_ctx_clear(cm_ctx_t *ctx)
{
    cm_node_t *new_root;
    if (!ctx) return;
    new_root = cm_node_new_object(NULL);
    if (!new_root) return;
    cm_node_free(ctx->root);
    ctx->root  = new_root;
    ctx->dirty = 0;
}

void cm_ctx_destroy(cm_ctx_t *ctx)
{
    if (!ctx) return;
    cm_node_free(ctx->root);
    free(ctx->source_path);
    free(ctx);
}

/* ═══════════════════════════════════════════════════════════════════
 * Key-path resolution
 * Supports:
 *   "key"              - top-level key
 *   "a.b.c"            - nested object
 *   "list[2]"          - array index
 *   "a.list[0].name"   - mixed
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    char   name[256];   /* segment key  */
    int    is_index;    /* 1 = array index */
    size_t index;
} cm_segment_t;

static int parse_keypath(const char *path, cm_segment_t *segs, int max_segs)
{
    int  count = 0;
    const char *p = path;

    while (*p && count < max_segs) {
        cm_segment_t *seg = &segs[count];
        memset(seg, 0, sizeof(*seg));

        /* read key name up to '.' or '[' */
        size_t i = 0;
        while (*p && *p != '.' && *p != '[' && i < 255) {
            seg->name[i++] = *p++;
        }
        seg->name[i] = '\0';

        /* array index? */
        if (*p == '[') {
            p++; /* skip '[' */
            char idx_buf[32] = {0};
            size_t j = 0;
            while (*p && *p != ']' && j < 31) idx_buf[j++] = *p++;
            if (*p == ']') p++;
            seg->is_index = 1;
            seg->index    = (size_t)atol(idx_buf);
        }

        count++;
        if (*p == '.') p++; /* skip '.' */
    }
    return count;
}

static cm_node_t *object_find_child(cm_node_t *obj, const char *key)
{
    if (!obj || obj->type != CM_TYPE_OBJECT) return NULL;
    for (size_t i = 0; i < obj->value.object.count; i++) {
        cm_node_t *c = obj->value.object.children[i];
        if (c->key && strcmp(c->key, key) == 0) return c;
    }
    return NULL;
}

cm_node_t *cm_get_node(cm_ctx_t *ctx, const char *key_path)
{
    if (!ctx || !key_path || !ctx->root) return NULL;

    cm_segment_t segs[64];
    int n = parse_keypath(key_path, segs, 64);
    if (n == 0) return NULL;

    cm_node_t *cur = ctx->root;

    for (int i = 0; i < n; i++) {
        cm_segment_t *s = &segs[i];

        if (cur->type == CM_TYPE_OBJECT) {
            cur = object_find_child(cur, s->name);
            if (!cur) return NULL;
        } else if (cur->type == CM_TYPE_ARRAY) {
            if (s->index >= cur->value.array.count) return NULL;
            cur = cur->value.array.items[s->index];
            if (!cur) return NULL;
        } else {
            return NULL;
        }

        /* handle [idx] suffix on the same segment */
        if (s->is_index && cur->type == CM_TYPE_ARRAY) {
            if (s->index >= cur->value.array.count) return NULL;
            cur = cur->value.array.items[s->index];
        }
    }
    return cur;
}

/* ═══════════════════════════════════════════════════════════════════
 * Ensure path (create intermediate objects as needed)
 * ═══════════════════════════════════════════════════════════════════ */

static cm_node_t *ensure_path(cm_ctx_t *ctx, const char *key_path,
                               const char **leaf_key_out)
{
    /* split last segment off */
    const char *last_dot = strrchr(key_path, '.');
    if (!last_dot) {
        *leaf_key_out = key_path;
        return ctx->root;
    }

    /* build parent path */
    char parent_path[512];
    size_t plen = (size_t)(last_dot - key_path);
    if (plen >= sizeof(parent_path)) return NULL;
    memcpy(parent_path, key_path, plen);
    parent_path[plen] = '\0';
    *leaf_key_out = last_dot + 1;

    /* walk / create */
    cm_segment_t segs[64];
    int n = parse_keypath(parent_path, segs, 64);
    cm_node_t *cur = ctx->root;
    for (int i = 0; i < n; i++) {
        if (!cur || cur->type != CM_TYPE_OBJECT) return NULL;
        cm_node_t *child = object_find_child(cur, segs[i].name);
        if (!child) {
            child = cm_node_new_object(segs[i].name);
            if (!child) return NULL;
            if (cm_node_object_add(cur, child) != CM_OK) {
                cm_node_free(child);
                return NULL;
            }
        }
        if (child->type != CM_TYPE_OBJECT) return NULL;
        cur = child;
    }
    return cur;
}

/* ═══════════════════════════════════════════════════════════════════
 * Set helpers (upsert)
 * ═══════════════════════════════════════════════════════════════════ */

static cm_error_t set_node(cm_ctx_t *ctx, const char *key_path, cm_node_t *newnode)
{
    cm_error_t result;
    if (!ctx || !ctx->root || !key_path || !newnode) {
        cm_node_free(newnode);
        return CM_ERR_NULL_PTR;
    }
    if (!*key_path) {
        cm_node_free(newnode);
        return CM_ERR_INVALID_KEY;
    }

    const char *leaf_key = NULL;
    cm_node_t  *parent   = ensure_path(ctx, key_path, &leaf_key);
    if (!parent) { cm_node_free(newnode); return CM_ERR_TYPE_MISMATCH; }
    if (parent->type != CM_TYPE_OBJECT) {
        cm_node_free(newnode);
        return CM_ERR_TYPE_MISMATCH;
    }
    if (!leaf_key || !*leaf_key) {
        cm_node_free(newnode);
        return CM_ERR_INVALID_KEY;
    }

    free(newnode->key);
    newnode->key = cm_internal_strdup(leaf_key);
    if (!newnode->key) {
        cm_node_free(newnode);
        return CM_ERR_NO_MEMORY;
    }

    /* replace existing or append */
    for (size_t i = 0; i < parent->value.object.count; i++) {
        if (strcmp(parent->value.object.children[i]->key, leaf_key) == 0) {
            cm_node_free(parent->value.object.children[i]);
            parent->value.object.children[i] = newnode;
            newnode->parent = parent;
            ctx->dirty = 1;
            return CM_OK;
        }
    }
    result = cm_node_object_add(parent, newnode);
    if (result != CM_OK) {
        cm_node_free(newnode);
        return result;
    }
    ctx->dirty = 1;
    return CM_OK;
}

cm_error_t cm_set_string(cm_ctx_t *ctx, const char *key, const char *val)
{
    if (!ctx || !key) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_node_new_string(NULL, val);
    if (!n) return CM_ERR_NO_MEMORY;
    return set_node(ctx, key, n);
}

cm_error_t cm_set_int(cm_ctx_t *ctx, const char *key, int64_t val)
{
    if (!ctx || !key) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_node_new_int(NULL, val);
    if (!n) return CM_ERR_NO_MEMORY;
    return set_node(ctx, key, n);
}

cm_error_t cm_set_float(cm_ctx_t *ctx, const char *key, double val)
{
    if (!ctx || !key) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_node_new_float(NULL, val);
    if (!n) return CM_ERR_NO_MEMORY;
    return set_node(ctx, key, n);
}

cm_error_t cm_set_bool(cm_ctx_t *ctx, const char *key, int val)
{
    if (!ctx || !key) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_node_new_bool(NULL, val);
    if (!n) return CM_ERR_NO_MEMORY;
    return set_node(ctx, key, n);
}

cm_error_t cm_set_null(cm_ctx_t *ctx, const char *key)
{
    if (!ctx || !key) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_node_new_null(NULL);
    if (!n) return CM_ERR_NO_MEMORY;
    return set_node(ctx, key, n);
}

/* ═══════════════════════════════════════════════════════════════════
 * Get helpers
 * ═══════════════════════════════════════════════════════════════════ */

cm_error_t cm_get_string(cm_ctx_t *ctx, const char *key, const char **out)
{
    if (!ctx || !key || !out) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_get_node(ctx, key);
    if (!n) return CM_ERR_NOT_FOUND;
    if (n->type == CM_TYPE_STRING) { *out = n->value.sval; return CM_OK; }
    return CM_ERR_TYPE_MISMATCH;
}

cm_error_t cm_get_int(cm_ctx_t *ctx, const char *key, int64_t *out)
{
    if (!ctx || !key || !out) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_get_node(ctx, key);
    if (!n) return CM_ERR_NOT_FOUND;
    if (n->type == CM_TYPE_INT)   { *out = n->value.ival; return CM_OK; }
    if (n->type == CM_TYPE_FLOAT) { *out = (int64_t)n->value.fval; return CM_OK; }
    if (n->type == CM_TYPE_STRING) {
        char *ep = NULL;
        *out = (int64_t)strtoll(n->value.sval, &ep, 10);
        if (ep != n->value.sval) return CM_OK;
    }
    return CM_ERR_TYPE_MISMATCH;
}

cm_error_t cm_get_float(cm_ctx_t *ctx, const char *key, double *out)
{
    if (!ctx || !key || !out) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_get_node(ctx, key);
    if (!n) return CM_ERR_NOT_FOUND;
    if (n->type == CM_TYPE_FLOAT) { *out = n->value.fval; return CM_OK; }
    if (n->type == CM_TYPE_INT)   { *out = (double)n->value.ival; return CM_OK; }
    if (n->type == CM_TYPE_STRING) {
        char *ep = NULL;
        *out = strtod(n->value.sval, &ep);
        if (ep != n->value.sval) return CM_OK;
    }
    return CM_ERR_TYPE_MISMATCH;
}

cm_error_t cm_get_bool(cm_ctx_t *ctx, const char *key, int *out)
{
    if (!ctx || !key || !out) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_get_node(ctx, key);
    if (!n) return CM_ERR_NOT_FOUND;
    if (n->type == CM_TYPE_BOOL)   { *out = n->value.bval; return CM_OK; }
    if (n->type == CM_TYPE_INT)    { *out = n->value.ival != 0; return CM_OK; }
    if (n->type == CM_TYPE_STRING) {
        const char *s = n->value.sval;
        if (strcmp(s,"true")==0 || strcmp(s,"yes")==0 || strcmp(s,"1")==0 ||
            strcmp(s,"on")==0   || strcmp(s,"TRUE")==0 || strcmp(s,"YES")==0) {
            *out = 1; return CM_OK;
        }
        if (strcmp(s,"false")==0|| strcmp(s,"no")==0  || strcmp(s,"0")==0 ||
            strcmp(s,"off")==0  || strcmp(s,"FALSE")==0|| strcmp(s,"NO")==0) {
            *out = 0; return CM_OK;
        }
    }
    return CM_ERR_TYPE_MISMATCH;
}

/* Defaults */
const char *cm_get_string_or(cm_ctx_t *ctx, const char *key, const char *def)
{ const char *v = NULL; return cm_get_string(ctx,key,&v)==CM_OK ? v : def; }
int64_t cm_get_int_or(cm_ctx_t *ctx, const char *key, int64_t def)
{ int64_t v = def; cm_get_int(ctx,key,&v); return v; }
double cm_get_float_or(cm_ctx_t *ctx, const char *key, double def)
{ double v = def; cm_get_float(ctx,key,&v); return v; }
int cm_get_bool_or(cm_ctx_t *ctx, const char *key, int def)
{ int v = def; cm_get_bool(ctx,key,&v); return v; }

/* ═══════════════════════════════════════════════════════════════════
 * Delete
 * ═══════════════════════════════════════════════════════════════════ */

cm_error_t cm_delete(cm_ctx_t *ctx, const char *key_path)
{
    if (!ctx || !key_path) return CM_ERR_NULL_PTR;
    if (!*key_path) return CM_ERR_INVALID_KEY;

    const char *leaf = key_path;
    cm_node_t *parent = ctx->root;
    const char *last_dot = strrchr(key_path, '.');
    if (last_dot) {
        char parent_path[512];
        size_t parent_length = (size_t)(last_dot - key_path);
        if (parent_length == 0 || parent_length >= sizeof(parent_path))
            return CM_ERR_INVALID_KEY;
        memcpy(parent_path, key_path, parent_length);
        parent_path[parent_length] = '\0';
        parent = cm_get_node(ctx, parent_path);
        leaf = last_dot + 1;
    }
    if (!*leaf) return CM_ERR_INVALID_KEY;
    if (!parent || parent->type != CM_TYPE_OBJECT) return CM_ERR_NOT_FOUND;

    for (size_t i = 0; i < parent->value.object.count; i++) {
        if (strcmp(parent->value.object.children[i]->key, leaf) == 0) {
            cm_node_free(parent->value.object.children[i]);
            /* shift */
            memmove(&parent->value.object.children[i],
                    &parent->value.object.children[i+1],
                    (parent->value.object.count - i - 1) * sizeof(cm_node_t *));
            parent->value.object.count--;
            ctx->dirty = 1;
            return CM_OK;
        }
    }
    return CM_ERR_NOT_FOUND;
}

int cm_has_key(cm_ctx_t *ctx, const char *key)
{
    return cm_get_node(ctx, key) != NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * Array helpers
 * ═══════════════════════════════════════════════════════════════════ */

cm_error_t cm_array_length(cm_ctx_t *ctx, const char *key, size_t *out)
{
    if (!ctx || !key || !out) return CM_ERR_NULL_PTR;
    cm_node_t *n = cm_get_node(ctx, key);
    if (!n) return CM_ERR_NOT_FOUND;
    if (n->type != CM_TYPE_ARRAY) return CM_ERR_TYPE_MISMATCH;
    *out = n->value.array.count;
    return CM_OK;
}

cm_node_t *cm_array_get(cm_ctx_t *ctx, const char *key, size_t index)
{
    cm_node_t *n = cm_get_node(ctx, key);
    if (!n || n->type != CM_TYPE_ARRAY) return NULL;
    if (index >= n->value.array.count)  return NULL;
    return n->value.array.items[index];
}

static cm_node_t *get_or_create_array(cm_ctx_t *ctx, const char *key)
{
    cm_node_t *n = cm_get_node(ctx, key);
    if (n) {
        if (n->type != CM_TYPE_ARRAY) return NULL;
        return n;
    }
    /* create */
    cm_node_t *arr = cm_node_new_array(NULL);
    if (!arr) return NULL;
    if (set_node(ctx, key, arr) != CM_OK) return NULL;
    return cm_get_node(ctx, key);
}

cm_error_t cm_array_push_string(cm_ctx_t *ctx, const char *key, const char *val)
{
    cm_node_t *arr = get_or_create_array(ctx, key);
    if (!arr) return CM_ERR_TYPE_MISMATCH;
    cm_node_t *item = cm_node_new_string(NULL, val);
    if (!item) return CM_ERR_NO_MEMORY;
    cm_error_t result = cm_node_array_add(arr, item);
    if (result != CM_OK) {
        cm_node_free(item);
        return result;
    }
    ctx->dirty = 1;
    return CM_OK;
}

cm_error_t cm_array_push_int(cm_ctx_t *ctx, const char *key, int64_t val)
{
    cm_node_t *arr = get_or_create_array(ctx, key);
    if (!arr) return CM_ERR_TYPE_MISMATCH;
    cm_node_t *item = cm_node_new_int(NULL, val);
    if (!item) return CM_ERR_NO_MEMORY;
    cm_error_t result = cm_node_array_add(arr, item);
    if (result != CM_OK) {
        cm_node_free(item);
        return result;
    }
    ctx->dirty = 1;
    return CM_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * Walk
 * ═══════════════════════════════════════════════════════════════════ */

static void walk_node(cm_node_t *node, char *path_buf, cm_walk_fn fn, void *ud)
{
    if (node->type == CM_TYPE_OBJECT) {
        for (size_t i = 0; i < node->value.object.count; i++) {
            cm_node_t *c = node->value.object.children[i];
            char child_path[1024];
            if (path_buf[0])
                snprintf(child_path, sizeof(child_path), "%s.%s", path_buf, c->key ? c->key : "");
            else
                snprintf(child_path, sizeof(child_path), "%s", c->key ? c->key : "");
            walk_node(c, child_path, fn, ud);
        }
    } else if (node->type == CM_TYPE_ARRAY) {
        for (size_t i = 0; i < node->value.array.count; i++) {
            char child_path[1024];
            snprintf(child_path, sizeof(child_path), "%s[%zu]", path_buf, i);
            walk_node(node->value.array.items[i], child_path, fn, ud);
        }
    } else {
        fn(path_buf, node, ud);
    }
}

void cm_walk(cm_ctx_t *ctx, cm_walk_fn fn, void *ud)
{
    if (!ctx || !fn || !ctx->root) return;
    char buf[1024] = {0};
    walk_node(ctx->root, buf, fn, ud);
}

/* ═══════════════════════════════════════════════════════════════════
 * Merge
 * ═══════════════════════════════════════════════════════════════════ */

static cm_node_t *node_deep_copy(cm_node_t *src);

static cm_node_t *node_deep_copy(cm_node_t *src)
{
    cm_error_t result;
    if (!src) return NULL;
    cm_node_t *dst = node_alloc();
    if (!dst) return NULL;
    dst->key  = cm_internal_strdup(src->key);
    if (src->key && !dst->key) { cm_node_free(dst); return NULL; }
    dst->type = src->type;
    switch (src->type) {
        case CM_TYPE_STRING:
            dst->value.sval = cm_internal_strdup(src->value.sval);
            if (src->value.sval && !dst->value.sval) {
                cm_node_free(dst);
                return NULL;
            }
            break;
        case CM_TYPE_INT:    dst->value.ival = src->value.ival; break;
        case CM_TYPE_FLOAT:  dst->value.fval = src->value.fval; break;
        case CM_TYPE_BOOL:   dst->value.bval = src->value.bval; break;
        case CM_TYPE_OBJECT:
            for (size_t i = 0; i < src->value.object.count; i++) {
                cm_node_t *c = node_deep_copy(src->value.object.children[i]);
                if (!c) { cm_node_free(dst); return NULL; }
                result = cm_node_object_add(dst, c);
                if (result != CM_OK) {
                    cm_node_free(c);
                    cm_node_free(dst);
                    return NULL;
                }
            }
            break;
        case CM_TYPE_ARRAY:
            for (size_t i = 0; i < src->value.array.count; i++) {
                cm_node_t *c = node_deep_copy(src->value.array.items[i]);
                if (!c) { cm_node_free(dst); return NULL; }
                result = cm_node_array_add(dst, c);
                if (result != CM_OK) {
                    cm_node_free(c);
                    cm_node_free(dst);
                    return NULL;
                }
            }
            break;
        default: break;
    }
    return dst;
}

static cm_error_t merge_objects(cm_node_t *dst, cm_node_t *src, int overwrite)
{
    for (size_t i = 0; i < src->value.object.count; i++) {
        cm_node_t *sc = src->value.object.children[i];
        cm_node_t *dc = object_find_child(dst, sc->key);

        if (!dc) {
            cm_node_t *copy = node_deep_copy(sc);
            if (!copy) return CM_ERR_NO_MEMORY;
            cm_error_t result = cm_node_object_add(dst, copy);
            if (result != CM_OK) {
                cm_node_free(copy);
                return result;
            }
        } else if (dc->type == CM_TYPE_OBJECT && sc->type == CM_TYPE_OBJECT) {
            cm_error_t result = merge_objects(dc, sc, overwrite);
            if (result != CM_OK) return result;
        } else if (overwrite) {
            /* replace */
            cm_node_t *copy = node_deep_copy(sc);
            if (!copy) return CM_ERR_NO_MEMORY;
            for (size_t j = 0; j < dst->value.object.count; j++) {
                if (dst->value.object.children[j] == dc) {
                    cm_node_free(dst->value.object.children[j]);
                    dst->value.object.children[j] = copy;
                    copy->parent = dst;
                    break;
                }
            }
        }
    }
    return CM_OK;
}

cm_error_t cm_merge(cm_ctx_t *dst, const cm_ctx_t *src, int overwrite)
{
    if (!dst || !src) return CM_ERR_NULL_PTR;
    if (!dst->root || !src->root || dst->root->type != CM_TYPE_OBJECT
            || src->root->type != CM_TYPE_OBJECT)
        return CM_ERR_TYPE_MISMATCH;
    cm_error_t result = merge_objects(dst->root, src->root, overwrite);
    if (result != CM_OK) return result;
    dst->dirty = 1;
    return CM_OK;
}

/* ═══════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════ */

const char *cm_error_str(cm_error_t err)
{
    switch (err) {
        case CM_OK:               return "OK";
        case CM_ERR_NULL_PTR:     return "Null pointer";
        case CM_ERR_NO_MEMORY:    return "Out of memory";
        case CM_ERR_NOT_FOUND:    return "Key not found";
        case CM_ERR_TYPE_MISMATCH:return "Type mismatch";
        case CM_ERR_PARSE:        return "Parse error";
        case CM_ERR_IO:           return "I/O error";
        case CM_ERR_UNSUPPORTED:  return "Unsupported format";
        case CM_ERR_INVALID_KEY:  return "Invalid key";
        case CM_ERR_OVERFLOW:     return "Buffer overflow";
        case CM_ERR_DUPLICATE:    return "Duplicate key";
        default:                  return "Unknown error";
    }
}

const char *cm_type_str(cm_type_t t)
{
    switch (t) {
        case CM_TYPE_NULL:   return "null";
        case CM_TYPE_BOOL:   return "bool";
        case CM_TYPE_INT:    return "int";
        case CM_TYPE_FLOAT:  return "float";
        case CM_TYPE_STRING: return "string";
        case CM_TYPE_ARRAY:  return "array";
        case CM_TYPE_OBJECT: return "object";
        default:             return "unknown";
    }
}

const char *cm_format_str(cm_format_t f)
{
    switch (f) {
        case CM_FORMAT_AUTO:       return "auto";
        case CM_FORMAT_JSON:       return "json";
        case CM_FORMAT_YAML:       return "yaml";
        case CM_FORMAT_XML:        return "xml";
        case CM_FORMAT_INI:        return "ini";
        case CM_FORMAT_TOML:       return "toml";
        case CM_FORMAT_ENV:        return "env";
        case CM_FORMAT_PROPERTIES: return "properties";
        default:                   return "unknown";
    }
}

cm_format_t cm_detect_format(const char *path)
{
    if (!path) return CM_FORMAT_AUTO;
    const char *ext = strrchr(path, '.');
    if (!ext || ext == path) return CM_FORMAT_AUTO;
    ext++;
    if (strcasecmp(ext, "json") == 0) return CM_FORMAT_JSON;
    if (strcasecmp(ext, "yaml") == 0 || strcasecmp(ext, "yml") == 0) return CM_FORMAT_YAML;
    if (strcasecmp(ext, "xml")  == 0) return CM_FORMAT_XML;
    if (strcasecmp(ext, "ini")  == 0 || strcasecmp(ext, "cfg") == 0) return CM_FORMAT_INI;
    if (strcasecmp(ext, "toml") == 0) return CM_FORMAT_TOML;
    if (strcasecmp(ext, "env")  == 0) return CM_FORMAT_ENV;
    if (strcasecmp(ext, "properties") == 0) return CM_FORMAT_PROPERTIES;
    return CM_FORMAT_AUTO;
}

/* ─── Debug dump ──────────────────────────────────────────────────── */

static void dump_walk(const char *path, cm_node_t *node, void *ud)
{
    (void)ud;
    switch (node->type) {
        case CM_TYPE_NULL:   printf("  %-40s = null\n", path); break;
        case CM_TYPE_BOOL:   printf("  %-40s = %s\n",   path, node->value.bval ? "true" : "false"); break;
        case CM_TYPE_INT:    printf("  %-40s = %lld\n", path, (long long)node->value.ival); break;
        case CM_TYPE_FLOAT:  printf("  %-40s = %g\n",   path, node->value.fval); break;
        case CM_TYPE_STRING: printf("  %-40s = \"%s\"\n", path, node->value.sval); break;
        default: break;
    }
}

void cm_dump(cm_ctx_t *ctx)
{
    printf("[config_manager dump — format=%s dirty=%d]\n",
           cm_format_str(ctx->format), ctx->dirty);
    cm_walk(ctx, dump_walk, NULL);
}
