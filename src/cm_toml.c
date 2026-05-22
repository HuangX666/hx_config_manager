/**
 * cm_toml.c  –  TOML load/save using tomlc99 (by CK Tan)
 *
 * Handles: scalar types, tables, arrays of tables, inline arrays/tables.
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toml.h"   /* tomlc99 */

/* ─── toml_table_t → cm_node_t (object) ─────────────────────────── */

static cm_node_t *toml_val_to_node(const char *key, toml_datum_t d, int kind);
static cm_node_t *table_to_node(const char *key, toml_table_t *tbl);
static cm_node_t *array_to_node(const char *key, toml_array_t *arr);

static cm_node_t *table_to_node(const char *key, toml_table_t *tbl)
{
    cm_node_t *obj = cm_node_new_object(key);
    if (!obj) return NULL;

    int n = toml_table_nkval(tbl)
          + toml_table_narr(tbl)
          + toml_table_ntab(tbl);
    (void)n;

    /* iterate keys */
    for (int i = 0; ; i++) {
        const char *k = toml_key_in(tbl, i);
        if (!k) break;

        toml_datum_t d;

        /* try scalar */
        d = toml_string_in(tbl, k);
        if (d.ok) {
            cm_node_t *c = cm_node_new_string(k, d.u.s);
            free(d.u.s);
            if (c) cm_node_object_add(obj, c);
            continue;
        }
        d = toml_bool_in(tbl, k);
        if (d.ok) {
            cm_node_t *c = cm_node_new_bool(k, d.u.b);
            if (c) cm_node_object_add(obj, c);
            continue;
        }
        d = toml_int_in(tbl, k);
        if (d.ok) {
            cm_node_t *c = cm_node_new_int(k, (int64_t)d.u.i);
            if (c) cm_node_object_add(obj, c);
            continue;
        }
        d = toml_double_in(tbl, k);
        if (d.ok) {
            cm_node_t *c = cm_node_new_float(k, d.u.d);
            if (c) cm_node_object_add(obj, c);
            continue;
        }

        /* try array */
        toml_array_t *arr = toml_array_in(tbl, k);
        if (arr) {
            cm_node_t *c = array_to_node(k, arr);
            if (c) cm_node_object_add(obj, c);
            continue;
        }

        /* try sub-table */
        toml_table_t *sub = toml_table_in(tbl, k);
        if (sub) {
            cm_node_t *c = table_to_node(k, sub);
            if (c) cm_node_object_add(obj, c);
            continue;
        }

        /* timestamp – store as string */
        d = toml_timestamp_in(tbl, k);
        if (d.ok) {
            char ts_buf[64] = "timestamp";
            toml_timestamp_t *ts = d.u.ts;
            if (ts) {
                snprintf(ts_buf, sizeof(ts_buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                    ts->year ? *ts->year : 0,
                    ts->month ? *ts->month : 0,
                    ts->day   ? *ts->day   : 0,
                    ts->hour  ? *ts->hour  : 0,
                    ts->minute? *ts->minute: 0,
                    ts->second? *ts->second: 0);
                free(ts);
            }
            cm_node_t *c = cm_node_new_string(k, ts_buf);
            if (c) cm_node_object_add(obj, c);
            continue;
        }
    }
    return obj;
}

static cm_node_t *array_to_node(const char *key, toml_array_t *arr)
{
    /* array of tables? */
    int kind = toml_array_kind(arr);   /* 't' table, 'v' value, 'a' array */

    cm_node_t *result = cm_node_new_array(key);
    if (!result) return NULL;

    if (kind == 't') {
        /* array of tables */
        for (int i = 0; ; i++) {
            toml_table_t *sub = toml_table_at(arr, i);
            if (!sub) break;
            cm_node_t *c = table_to_node(NULL, sub);
            if (c) cm_node_array_add(result, c);
        }
        return result;
    }

    if (kind == 'a') {
        for (int i = 0; ; i++) {
            toml_array_t *sub = toml_array_at(arr, i);
            if (!sub) break;
            cm_node_t *c = array_to_node(NULL, sub);
            if (c) cm_node_array_add(result, c);
        }
        return result;
    }

    /* value array */
    char type = toml_array_type(arr);   /* 'i','d','b','s','t' */
    for (int i = 0; ; i++) {
        toml_datum_t d;
        cm_node_t *item = NULL;

        switch (type) {
            case 's': d = toml_string_at(arr, i);
                      if (!d.ok) goto done;
                      item = cm_node_new_string(NULL, d.u.s);
                      free(d.u.s); break;
            case 'b': d = toml_bool_at(arr, i);
                      if (!d.ok) goto done;
                      item = cm_node_new_bool(NULL, d.u.b); break;
            case 'i': d = toml_int_at(arr, i);
                      if (!d.ok) goto done;
                      item = cm_node_new_int(NULL, (int64_t)d.u.i); break;
            case 'd': d = toml_double_at(arr, i);
                      if (!d.ok) goto done;
                      item = cm_node_new_float(NULL, d.u.d); break;
            default:  goto done;
        }
        if (item) cm_node_array_add(result, item);
    }
done:
    return result;
}

/* ─── Load ───────────────────────────────────────────────────────── */

cm_error_t cm_toml_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;

    char errbuf[256] = {0};
    toml_table_t *tbl = toml_parse((char *)data, errbuf, sizeof(errbuf));
    if (!tbl) {
        fprintf(stderr, "[cm_toml] parse error: %s\n", errbuf);
        return CM_ERR_PARSE;
    }

    cm_ctx_clear(ctx);
    cm_node_t *obj = table_to_node(NULL, tbl);
    toml_free(tbl);

    if (!obj) return CM_ERR_NO_MEMORY;
    /* merge children into root */
    for (size_t i = 0; i < obj->value.object.count; i++)
        cm_node_object_add(ctx->root, obj->value.object.children[i]);
    obj->value.object.count = 0;
    cm_node_free(obj);

    ctx->format = CM_FORMAT_TOML;
    return CM_OK;
}

cm_error_t cm_toml_load_file(cm_ctx_t *ctx, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return CM_ERR_IO;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return CM_ERR_NO_MEMORY; }
    fread(buf, 1, (size_t)sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    cm_error_t err = cm_toml_load_string(ctx, buf);
    free(buf);
    return err;
}

/* ─── Serializer ─────────────────────────────────────────────────── */

typedef struct { char *buf; size_t len; size_t cap; } toml_out_t;

static void tout_append(toml_out_t *o, const char *s)
{
    size_t sl = strlen(s);
    if (o->len + sl + 1 > o->cap) {
        size_t nc = o->cap ? o->cap * 2 : 4096;
        while (nc < o->len + sl + 1) nc *= 2;
        char *p = (char *)realloc(o->buf, nc);
        if (!p) return;
        o->buf = p; o->cap = nc;
    }
    memcpy(o->buf + o->len, s, sl);
    o->len += sl;
    o->buf[o->len] = '\0';
}

static void emit_toml_node(toml_out_t *out, cm_node_t *n,
                            const char *section_prefix, int depth);

static void emit_toml_value(toml_out_t *out, cm_node_t *n)
{
    char buf[64];
    switch (n->type) {
        case CM_TYPE_NULL:   tout_append(out, "\"\""); break;
        case CM_TYPE_BOOL:   tout_append(out, n->value.bval?"true":"false"); break;
        case CM_TYPE_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)n->value.ival);
            tout_append(out, buf); break;
        case CM_TYPE_FLOAT:
            snprintf(buf, sizeof(buf), "%g", n->value.fval);
            tout_append(out, buf); break;
        case CM_TYPE_STRING:
            tout_append(out, "\"");
            /* escape backslash and double-quote */
            if (n->value.sval) {
                for (const char *p = n->value.sval; *p; p++) {
                    if (*p == '"')  tout_append(out, "\\\"");
                    else if (*p == '\\') tout_append(out, "\\\\");
                    else { char c[2] = {*p, 0}; tout_append(out, c); }
                }
            }
            tout_append(out, "\""); break;
        case CM_TYPE_ARRAY: {
            tout_append(out, "[");
            for (size_t i = 0; i < n->value.array.count; i++) {
                if (i) tout_append(out, ", ");
                emit_toml_value(out, n->value.array.items[i]);
            }
            tout_append(out, "]"); break;
        }
        default: tout_append(out, "\"\""); break;
    }
}

static void emit_toml_table(toml_out_t *out, cm_node_t *obj,
                              const char *section_path)
{
    /* first pass: scalar/array values */
    for (size_t i = 0; i < obj->value.object.count; i++) {
        cm_node_t *c = obj->value.object.children[i];
        if (c->type == CM_TYPE_OBJECT) continue;
        if (c->type == CM_TYPE_ARRAY) {
            /* check if array of tables */
            int aot = (c->value.array.count > 0 &&
                       c->value.array.items[0]->type == CM_TYPE_OBJECT);
            if (aot) continue;
        }
        tout_append(out, c->key ? c->key : "value");
        tout_append(out, " = ");
        emit_toml_value(out, c);
        tout_append(out, "\n");
    }

    /* second pass: sub-tables */
    for (size_t i = 0; i < obj->value.object.count; i++) {
        cm_node_t *c = obj->value.object.children[i];
        if (c->type != CM_TYPE_OBJECT) {
            if (c->type == CM_TYPE_ARRAY && c->value.array.count > 0 &&
                c->value.array.items[0]->type == CM_TYPE_OBJECT) {
                /* array of tables */
                char path[512];
                if (section_path && *section_path)
                    snprintf(path, sizeof(path), "%s.%s", section_path, c->key);
                else
                    snprintf(path, sizeof(path), "%s", c->key ? c->key : "");
                for (size_t j = 0; j < c->value.array.count; j++) {
                    tout_append(out, "\n[[");
                    tout_append(out, path);
                    tout_append(out, "]]\n");
                    emit_toml_table(out, c->value.array.items[j], path);
                }
            }
            continue;
        }

        char path[512];
        if (section_path && *section_path)
            snprintf(path, sizeof(path), "%s.%s", section_path, c->key ? c->key : "");
        else
            snprintf(path, sizeof(path), "%s", c->key ? c->key : "");

        tout_append(out, "\n[");
        tout_append(out, path);
        tout_append(out, "]\n");
        emit_toml_table(out, c, path);
    }
}

char *cm_toml_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;
    toml_out_t out = {NULL, 0, 0};
    emit_toml_table(&out, ctx->root, "");
    if (out_len) *out_len = out.len;
    return out.buf ? out.buf : strdup("");
}

cm_error_t cm_toml_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len = 0;
    char  *str = cm_toml_save_string(ctx, &len);
    if (!str) return CM_ERR_NO_MEMORY;
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(str); return CM_ERR_IO; }
    fwrite(str, 1, len, fp);
    fclose(fp);
    free(str);
    return CM_OK;
}
