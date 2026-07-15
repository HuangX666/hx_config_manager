/**
 * cm_yaml.c  –  YAML load/save via libyaml
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

/* ─── Parser state ───────────────────────────────────────────────── */

typedef struct {
    yaml_parser_t  parser;
    yaml_event_t   event;
} yaml_ctx_t;

static void yctx_next(yaml_ctx_t *yc)
{
    yaml_event_delete(&yc->event);
    if (!yaml_parser_parse(&yc->parser, &yc->event)) {
        /* fill with STREAM_END on error */
        yc->event.type = YAML_STREAM_END_EVENT;
    }
}

static cm_node_t *parse_value(yaml_ctx_t *yc, const char *key);

static cm_node_t *parse_sequence(yaml_ctx_t *yc, const char *key)
{
    cm_node_t *arr = cm_node_new_array(key);
    if (!arr) return NULL;

    yctx_next(yc); /* consume SEQUENCE_START */

    while (yc->event.type != YAML_SEQUENCE_END_EVENT &&
           yc->event.type != YAML_STREAM_END_EVENT)
    {
        cm_node_t *item = parse_value(yc, NULL);
        if (item) cm_node_array_add(arr, item);
    }
    yctx_next(yc); /* consume SEQUENCE_END */
    return arr;
}

static cm_node_t *parse_mapping(yaml_ctx_t *yc, const char *key)
{
    cm_node_t *obj = cm_node_new_object(key);
    if (!obj) return NULL;

    yctx_next(yc); /* consume MAPPING_START */

    while (yc->event.type != YAML_MAPPING_END_EVENT &&
           yc->event.type != YAML_STREAM_END_EVENT)
    {
        /* key */
        if (yc->event.type != YAML_SCALAR_EVENT) { yctx_next(yc); continue; }
        char *ckey = cm_internal_strdup((char *)yc->event.data.scalar.value);
        yctx_next(yc);

        cm_node_t *val = parse_value(yc, ckey);
        free(ckey);
        if (val) cm_node_object_add(obj, val);
    }
    yctx_next(yc); /* consume MAPPING_END */
    return obj;
}

/* Convert a YAML scalar string to the most fitting cm_node_t */
static cm_node_t *scalar_to_node(const char *key, const char *val)
{
    if (!val || strcmp(val,"null")==0 || strcmp(val,"~")==0) return cm_node_new_null(key);
    if (strcmp(val,"true")==0  || strcmp(val,"yes")==0 || strcmp(val,"on")==0)
        return cm_node_new_bool(key, 1);
    if (strcmp(val,"false")==0 || strcmp(val,"no")==0  || strcmp(val,"off")==0)
        return cm_node_new_bool(key, 0);

    /* try integer */
    char *ep = NULL;
    int64_t ival = strtoll(val, &ep, 0);
    if (ep && *ep == '\0' && ep != val) return cm_node_new_int(key, ival);

    /* try float */
    double fval = strtod(val, &ep);
    if (ep && *ep == '\0' && ep != val) return cm_node_new_float(key, fval);

    return cm_node_new_string(key, val);
}

static cm_node_t *parse_value(yaml_ctx_t *yc, const char *key)
{
    switch (yc->event.type) {
        case YAML_SCALAR_EVENT: {
            cm_node_t *n = scalar_to_node(key, (char *)yc->event.data.scalar.value);
            yctx_next(yc);
            return n;
        }
        case YAML_SEQUENCE_START_EVENT:
            return parse_sequence(yc, key);
        case YAML_MAPPING_START_EVENT:
            return parse_mapping(yc, key);
        default:
            yctx_next(yc);
            return NULL;
    }
}

/* ─── Public load ───────────────────────────────────────────────── */

static cm_error_t yaml_parse_input(cm_ctx_t *ctx, yaml_ctx_t *yc)
{
    yctx_next(yc); /* STREAM_START */
    if (yc->event.type != YAML_STREAM_START_EVENT) return CM_ERR_PARSE;
    yctx_next(yc); /* DOCUMENT_START */
    if (yc->event.type != YAML_DOCUMENT_START_EVENT) return CM_ERR_PARSE;
    yctx_next(yc); /* first value event */

    cm_ctx_clear(ctx);

    if (yc->event.type == YAML_MAPPING_START_EVENT) {
        cm_node_t *obj = parse_mapping(yc, NULL);
        if (obj) {
            /* merge children into root */
            for (size_t i = 0; i < obj->value.object.count; i++)
                cm_node_object_add(ctx->root, obj->value.object.children[i]);
            obj->value.object.count = 0;
            cm_node_free(obj);
        }
    } else if (yc->event.type == YAML_SEQUENCE_START_EVENT) {
        cm_node_t *arr = parse_sequence(yc, "root");
        if (arr) cm_node_object_add(ctx->root, arr);
    } else {
        cm_node_t *n = parse_value(yc, "root");
        if (n) cm_node_object_add(ctx->root, n);
    }

    ctx->format = CM_FORMAT_YAML;
    return CM_OK;
}

cm_error_t cm_yaml_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;

    yaml_ctx_t yc;
    memset(&yc, 0, sizeof(yc));
    yaml_parser_initialize(&yc.parser);
    yaml_parser_set_input_string(&yc.parser,
        (const unsigned char *)data, strlen(data));

    cm_error_t err = yaml_parse_input(ctx, &yc);

    yaml_event_delete(&yc.event);
    yaml_parser_delete(&yc.parser);
    return err;
}

cm_error_t cm_yaml_load_file(cm_ctx_t *ctx, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return CM_ERR_IO;

    yaml_ctx_t yc;
    memset(&yc, 0, sizeof(yc));
    yaml_parser_initialize(&yc.parser);
    yaml_parser_set_input_file(&yc.parser, fp);

    cm_error_t err = yaml_parse_input(ctx, &yc);

    yaml_event_delete(&yc.event);
    yaml_parser_delete(&yc.parser);
    fclose(fp);
    return err;
}

/* ─── Serializer ─────────────────────────────────────────────────── */

typedef struct {
    yaml_emitter_t  emitter;
    yaml_event_t    ev;
} yaml_emit_ctx_t;

#define YE_EMIT(ectx) yaml_emitter_emit(&(ectx)->emitter, &(ectx)->ev)

static void emit_scalar(yaml_emit_ctx_t *ec, const char *val, int is_key)
{
    yaml_scalar_event_initialize(&ec->ev,
        NULL, (yaml_char_t *)YAML_STR_TAG,
        (yaml_char_t *)val, (int)strlen(val),
        1, 1,
        is_key ? YAML_PLAIN_SCALAR_STYLE : YAML_ANY_SCALAR_STYLE);
    YE_EMIT(ec);
}

static void emit_node(yaml_emit_ctx_t *ec, cm_node_t *n);

static void emit_node(yaml_emit_ctx_t *ec, cm_node_t *n)
{
    if (!n) return;
    char buf[64];
    switch (n->type) {
        case CM_TYPE_NULL:
            emit_scalar(ec, "null", 0);
            break;
        case CM_TYPE_BOOL:
            emit_scalar(ec, n->value.bval ? "true" : "false", 0);
            break;
        case CM_TYPE_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)n->value.ival);
            emit_scalar(ec, buf, 0);
            break;
        case CM_TYPE_FLOAT:
            snprintf(buf, sizeof(buf), "%g", n->value.fval);
            emit_scalar(ec, buf, 0);
            break;
        case CM_TYPE_STRING:
            emit_scalar(ec, n->value.sval ? n->value.sval : "", 0);
            break;
        case CM_TYPE_OBJECT: {
            yaml_mapping_start_event_initialize(&ec->ev,
                NULL, (yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
            YE_EMIT(ec);
            for (size_t i = 0; i < n->value.object.count; i++) {
                cm_node_t *c = n->value.object.children[i];
                emit_scalar(ec, c->key ? c->key : "", 1);
                emit_node(ec, c);
            }
            yaml_mapping_end_event_initialize(&ec->ev);
            YE_EMIT(ec);
            break;
        }
        case CM_TYPE_ARRAY: {
            yaml_sequence_start_event_initialize(&ec->ev,
                NULL, (yaml_char_t *)YAML_SEQ_TAG, 1, YAML_BLOCK_SEQUENCE_STYLE);
            YE_EMIT(ec);
            for (size_t i = 0; i < n->value.array.count; i++)
                emit_node(ec, n->value.array.items[i]);
            yaml_sequence_end_event_initialize(&ec->ev);
            YE_EMIT(ec);
            break;
        }
        default: break;
    }
}

typedef struct { char *buf; size_t len; size_t cap; } yaml_out_buf_t;

static int yaml_write_handler(void *data, unsigned char *buffer, size_t size)
{
    yaml_out_buf_t *ob = (yaml_out_buf_t *)data;
    if (ob->len + size + 1 > ob->cap) {
        size_t nc = ob->cap ? ob->cap * 2 : 4096;
        while (nc < ob->len + size + 1) nc *= 2;
        char *p = (char *)realloc(ob->buf, nc);
        if (!p) return 0;
        ob->buf = p; ob->cap = nc;
    }
    memcpy(ob->buf + ob->len, buffer, size);
    ob->len += size;
    ob->buf[ob->len] = '\0';
    return 1;
}

char *cm_yaml_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;

    yaml_emit_ctx_t ec;
    memset(&ec, 0, sizeof(ec));
    yaml_emitter_initialize(&ec.emitter);

    yaml_out_buf_t ob = {NULL, 0, 0};
    yaml_emitter_set_output(&ec.emitter, yaml_write_handler, &ob);
    yaml_emitter_set_unicode(&ec.emitter, 1);

    /* STREAM_START */
    yaml_stream_start_event_initialize(&ec.ev, YAML_UTF8_ENCODING);
    YE_EMIT(&ec);
    /* DOCUMENT_START */
    yaml_document_start_event_initialize(&ec.ev, NULL, NULL, NULL, 0);
    YE_EMIT(&ec);

    emit_node(&ec, ctx->root);

    /* DOCUMENT_END */
    yaml_document_end_event_initialize(&ec.ev, 1);
    YE_EMIT(&ec);
    /* STREAM_END */
    yaml_stream_end_event_initialize(&ec.ev);
    YE_EMIT(&ec);

    yaml_emitter_delete(&ec.emitter);

    if (out_len) *out_len = ob.len;
    return ob.buf;
}

cm_error_t cm_yaml_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len = 0;
    char  *str = cm_yaml_save_string(ctx, &len);
    if (!str) return CM_ERR_NO_MEMORY;

    cm_error_t result = cm_internal_write_file(path, str, len);
    free(str);
    return result;
}
