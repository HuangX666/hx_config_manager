/**
 * cm_json.c  –  JSON load/save via cJSON
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* ─── cJSON → cm_node_t ─────────────────────────────────────────── */

static cm_node_t *cjson_to_node(const char *key, cJSON *j)
{
    if (cJSON_IsNull(j))    return cm_node_new_null(key);
    if (cJSON_IsBool(j))    return cm_node_new_bool(key, cJSON_IsTrue(j));
    if (cJSON_IsNumber(j)) {
        double d = j->valuedouble;
        /* prefer int if value is integral */
        if (d == (double)(int64_t)d)
            return cm_node_new_int(key, (int64_t)d);
        return cm_node_new_float(key, d);
    }
    if (cJSON_IsString(j))  return cm_node_new_string(key, j->valuestring);

    if (cJSON_IsObject(j)) {
        cm_node_t *obj = cm_node_new_object(key);
        if (!obj) return NULL;
        cJSON *child = NULL;
        cJSON_ArrayForEach(child, j) {
            cm_node_t *c = cjson_to_node(child->string, child);
            if (c) cm_node_object_add(obj, c);
        }
        return obj;
    }

    if (cJSON_IsArray(j)) {
        cm_node_t *arr = cm_node_new_array(key);
        if (!arr) return NULL;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, j) {
            cm_node_t *c = cjson_to_node(NULL, item);
            if (c) cm_node_array_add(arr, c);
        }
        return arr;
    }

    return cm_node_new_null(key);
}

/* ─── cm_node_t → cJSON ─────────────────────────────────────────── */

static cJSON *node_to_cjson(cm_node_t *n)
{
    switch (n->type) {
        case CM_TYPE_NULL:   return cJSON_CreateNull();
        case CM_TYPE_BOOL:   return cJSON_CreateBool(n->value.bval);
        case CM_TYPE_INT:    return cJSON_CreateNumber((double)n->value.ival);
        case CM_TYPE_FLOAT:  return cJSON_CreateNumber(n->value.fval);
        case CM_TYPE_STRING: return cJSON_CreateString(n->value.sval ? n->value.sval : "");
        case CM_TYPE_OBJECT: {
            cJSON *obj = cJSON_CreateObject();
            for (size_t i = 0; i < n->value.object.count; i++) {
                cm_node_t *c = n->value.object.children[i];
                cJSON *child = node_to_cjson(c);
                if (child) cJSON_AddItemToObject(obj, c->key ? c->key : "", child);
            }
            return obj;
        }
        case CM_TYPE_ARRAY: {
            cJSON *arr = cJSON_CreateArray();
            for (size_t i = 0; i < n->value.array.count; i++) {
                cJSON *item = node_to_cjson(n->value.array.items[i]);
                if (item) cJSON_AddItemToArray(arr, item);
            }
            return arr;
        }
        default: return cJSON_CreateNull();
    }
}

/* ─── Public API ─────────────────────────────────────────────────── */

cm_error_t cm_json_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;

    cJSON *root = cJSON_Parse(data);
    if (!root) {
        const char *ep = cJSON_GetErrorPtr();
        if (ep) fprintf(stderr, "[cm_json] parse error near: %.40s\n", ep);
        return CM_ERR_PARSE;
    }

    cm_ctx_clear(ctx);

    if (cJSON_IsObject(root)) {
        cJSON *child = NULL;
        cJSON_ArrayForEach(child, root) {
            cm_node_t *n = cjson_to_node(child->string, child);
            if (n) cm_node_object_add(ctx->root, n);
        }
    } else {
        /* top-level array or scalar — wrap under "root" key */
        cm_node_t *n = cjson_to_node("root", root);
        if (n) cm_node_object_add(ctx->root, n);
    }

    cJSON_Delete(root);
    ctx->format = CM_FORMAT_JSON;
    return CM_OK;
}

cm_error_t cm_json_load_file(cm_ctx_t *ctx, const char *path)
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

    cm_error_t err = cm_json_load_string(ctx, buf);
    free(buf);
    return err;
}

char *cm_json_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;

    cJSON *root = node_to_cjson(ctx->root);
    if (!root) return NULL;

    char *str = cJSON_Print(root);
    cJSON_Delete(root);

    if (out_len && str) *out_len = strlen(str);
    return str;
}

cm_error_t cm_json_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len = 0;
    char  *str = cm_json_save_string(ctx, &len);
    if (!str) return CM_ERR_NO_MEMORY;

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(str); return CM_ERR_IO; }
    fwrite(str, 1, len, fp);
    fclose(fp);
    free(str);
    return CM_OK;
}
