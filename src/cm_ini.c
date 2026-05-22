/**
 * cm_ini.c  –  INI load/save via inih
 *
 * Convention:
 *   [section]
 *   key = value
 *
 * Top-level (no section) keys mapped under "" or directly if no conflict.
 * Resulting path: "section.key"  or just "key" for global entries.
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ini.h"   /* inih */

/* ─── inih handler ───────────────────────────────────────────────── */

typedef struct {
    cm_ctx_t   *ctx;
    cm_error_t  err;
} ini_handler_data_t;

static int ini_callback(void *user, const char *section,
                       const char *name, const char *value)
{
    ini_handler_data_t *hd = (ini_handler_data_t *)user;
    if (hd->err != CM_OK) return 0;

    char key[512];
    if (section && *section)
        snprintf(key, sizeof(key), "%s.%s", section, name);
    else
        snprintf(key, sizeof(key), "%s", name);

    /* auto-detect types */
    if (strcmp(value,"true")==0  || strcmp(value,"yes")==0 ||
        strcmp(value,"on")==0    || strcmp(value,"TRUE")==0) {
        hd->err = cm_set_bool(hd->ctx, key, 1); return 1;
    }
    if (strcmp(value,"false")==0 || strcmp(value,"no")==0  ||
        strcmp(value,"off")==0   || strcmp(value,"FALSE")==0) {
        hd->err = cm_set_bool(hd->ctx, key, 0); return 1;
    }

    char *ep = NULL;
    int64_t iv = strtoll(value, &ep, 10);
    if (ep && *ep=='\0' && ep!=value) {
        hd->err = cm_set_int(hd->ctx, key, iv); return 1;
    }

    double dv = strtod(value, &ep);
    if (ep && *ep=='\0' && ep!=value) {
        hd->err = cm_set_float(hd->ctx, key, dv); return 1;
    }

    hd->err = cm_set_string(hd->ctx, key, value);
    return 1;
}

/* ─── Load ───────────────────────────────────────────────────────── */

cm_error_t cm_ini_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;
    cm_ctx_clear(ctx);

    ini_handler_data_t hd = { ctx, CM_OK };
    int r = ini_parse_string(data, ini_callback, &hd);
    if (r < 0) return CM_ERR_PARSE;
    if (hd.err != CM_OK) return hd.err;

    ctx->format = CM_FORMAT_INI;
    return CM_OK;
}

cm_error_t cm_ini_load_file(cm_ctx_t *ctx, const char *path)
{
    if (!ctx || !path) return CM_ERR_NULL_PTR;
    cm_ctx_clear(ctx);

    ini_handler_data_t hd = { ctx, CM_OK };
    int r = ini_parse(path, ini_callback, &hd);
    if (r < 0) return CM_ERR_IO;
    if (r > 0) return CM_ERR_PARSE;
    if (hd.err != CM_OK) return hd.err;

    ctx->format = CM_FORMAT_INI;
    return CM_OK;
}

/* ─── Save ───────────────────────────────────────────────────────── */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} ini_buf_t;

static void ini_buf_append(ini_buf_t *b, const char *s)
{
    size_t sl = strlen(s);
    if (b->len + sl + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->len + sl + 1) nc *= 2;
        char *p = (char *)realloc(b->buf, nc);
        if (!p) return;
        b->buf = p; b->cap = nc;
    }
    memcpy(b->buf + b->len, s, sl);
    b->len += sl;
    b->buf[b->len] = '\0';
}

/* Walk the tree emitting INI text.
   Two-level only: global keys then [section] keys. Deeper nesting is
   flattened with dot notation as a comment-annotated catch-all. */
typedef struct {
    ini_buf_t  *out;
    char        cur_section[256];
} ini_emit_t;

static void ini_emit_walk(const char *path, cm_node_t *node, void *ud)
{
    ini_emit_t *em = (ini_emit_t *)ud;
    char section[256] = {0};
    const char *key = path;
    char val[1024] = {0};

    /* split last component as key */
    const char *dot = strchr(path, '.');
    if (dot) {
        snprintf(section, sizeof(section), "%.*s", (int)(dot - path), path);
        key = dot + 1;
    }

    /* section header change */
    if (strcmp(section, em->cur_section) != 0) {
        snprintf(em->cur_section, sizeof(em->cur_section), "%s", section);
        if (section[0]) {
            ini_buf_append(em->out, "\n[");
            ini_buf_append(em->out, section);
            ini_buf_append(em->out, "]\n");
        }
    }

    switch (node->type) {
        case CM_TYPE_NULL:   snprintf(val, sizeof(val), ""); break;
        case CM_TYPE_BOOL:   snprintf(val, sizeof(val), "%s", node->value.bval?"true":"false"); break;
        case CM_TYPE_INT:    snprintf(val, sizeof(val), "%lld", (long long)node->value.ival); break;
        case CM_TYPE_FLOAT:  snprintf(val, sizeof(val), "%g", node->value.fval); break;
        case CM_TYPE_STRING: snprintf(val, sizeof(val), "%s", node->value.sval ? node->value.sval : ""); break;
        default: return; /* skip nested objects/arrays inline */
    }

    ini_buf_append(em->out, key);
    ini_buf_append(em->out, " = ");
    ini_buf_append(em->out, val);
    ini_buf_append(em->out, "\n");
}

/* Recursively walk tree for INI output.
   At each level, leaf children are emitted BEFORE object children,
   ensuring global (no-section) keys always appear before [section]
   headers. This prevents global keys from being incorrectly nested
   under a preceding section. */
static void ini_walk_tree(cm_node_t *node, const char *prefix,
                          cm_walk_fn fn, void *ud)
{
    if (!node) return;

    if (node->type != CM_TYPE_OBJECT && node->type != CM_TYPE_ARRAY) {
        fn(prefix, node, ud);
        return;
    }

    if (node->type == CM_TYPE_ARRAY) {
        for (size_t i = 0; i < node->value.array.count; i++) {
            char child_path[1024];
            snprintf(child_path, sizeof(child_path), "%s[%zu]", prefix, i);
            ini_walk_tree(node->value.array.items[i], child_path, fn, ud);
        }
        return;
    }

    /* OBJECT: pass 1 — leaf children first */
    for (size_t i = 0; i < node->value.object.count; i++) {
        cm_node_t *c = node->value.object.children[i];
        if (c->type != CM_TYPE_OBJECT && c->type != CM_TYPE_ARRAY) {
            char child_path[1024];
            if (prefix[0])
                snprintf(child_path, sizeof(child_path), "%s.%s", prefix,
                         c->key ? c->key : "");
            else
                snprintf(child_path, sizeof(child_path), "%s",
                         c->key ? c->key : "");
            fn(child_path, c, ud);
        }
    }

    /* OBJECT: pass 2 — object / array children */
    for (size_t i = 0; i < node->value.object.count; i++) {
        cm_node_t *c = node->value.object.children[i];
        if (c->type == CM_TYPE_OBJECT || c->type == CM_TYPE_ARRAY) {
            char child_path[1024];
            if (prefix[0])
                snprintf(child_path, sizeof(child_path), "%s.%s", prefix,
                         c->key ? c->key : "");
            else
                snprintf(child_path, sizeof(child_path), "%s",
                         c->key ? c->key : "");
            ini_walk_tree(c, child_path, fn, ud);
        }
    }
}

char *cm_ini_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;
    ini_buf_t out = {NULL, 0, 0};
    ini_emit_t em;
    memset(&em, 0, sizeof(em));
    em.out = &out;

    /* Walk tree: leaves-before-objects ensures global keys come first */
    ini_walk_tree(ctx->root, "", ini_emit_walk, &em);

    if (out_len) *out_len = out.len;
    return out.buf ? out.buf : strdup("");
}

cm_error_t cm_ini_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len = 0;
    char  *str = cm_ini_save_string(ctx, &len);
    if (!str) return CM_ERR_NO_MEMORY;

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(str); return CM_ERR_IO; }
    fwrite(str, 1, len, fp);
    fclose(fp);
    free(str);
    return CM_OK;
}
