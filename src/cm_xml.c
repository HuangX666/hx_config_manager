/**
 * cm_xml.c  –  XML load/save via Mini-XML (mxml)
 *
 * Convention:
 *   <config>
 *     <server>
 *       <host>localhost</host>
 *       <port>8080</port>
 *     </server>
 *     <tags type="array">
 *       <item>a</item>
 *       <item>b</item>
 *     </tags>
 *   </config>
 *
 * Arrays: elements with attribute type="array" or repeated sibling names.
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <mxml.h>

/* ─── Helpers ────────────────────────────────────────────────────── */

static const char *get_text(mxml_node_t *node)
{
#if MXML_MAJOR_VERSION >= 4
    return mxmlGetOpaque(node);
#else
    mxml_node_t *child = mxmlGetFirstChild(node);
    if (child && mxmlGetType(child) == MXML_TEXT)
        return mxmlGetText(child, NULL);
    if (child && mxmlGetType(child) == MXML_OPAQUE)
        return mxmlGetOpaque(child);
    return NULL;
#endif
}

static const char *trim(const char *s, char *buf, size_t blen)
{
    if (!s) return "";
    while (isspace((unsigned char)*s)) s++;
    size_t l = strlen(s);
    while (l > 0 && isspace((unsigned char)s[l-1])) l--;
    if (l >= blen) l = blen - 1;
    memcpy(buf, s, l);
    buf[l] = '\0';
    return buf;
}

/* detect numeric/bool types for text content */
static cm_node_t *text_to_node(const char *key, const char *raw)
{
    char buf[512];
    const char *val = trim(raw, buf, sizeof(buf));
    if (!*val) return cm_node_new_string(key, val);

    if (strcmp(val,"true")==0 || strcmp(val,"yes")==0) return cm_node_new_bool(key,1);
    if (strcmp(val,"false")==0|| strcmp(val,"no")==0)  return cm_node_new_bool(key,0);
    if (strcmp(val,"null")==0) return cm_node_new_null(key);

    char *ep = NULL;
    int64_t iv = strtoll(val, &ep, 10);
    if (ep && *ep=='\0' && ep!=val) return cm_node_new_int(key, iv);

    double dv = strtod(val, &ep);
    if (ep && *ep=='\0' && ep!=val) return cm_node_new_float(key, dv);

    return cm_node_new_string(key, val);
}

/* ─── mxml element → cm_node_t ─────────────────────────────────── */

static cm_node_t *mxml_to_node(mxml_node_t *el)
{
    const char *name = mxmlGetElement(el);
    if (!name) return NULL;

    /* explicit type="array" attribute? */
    const char *type_attr = mxmlElementGetAttr(el, "type");
    int force_array = (type_attr && strcmp(type_attr,"array")==0);

    /* count direct element children */
    int child_count = 0;
    mxml_node_t *c = mxmlGetFirstChild(el);
    while (c) {
        if (mxmlGetType(c) == MXML_TYPE_ELEMENT) child_count++;
        c = mxmlGetNextSibling(c);
    }

    if (child_count == 0) {
        /* leaf node */
        const char *txt = get_text(el);
        return text_to_node(name, txt ? txt : "");
    }

    /* detect array: all children share same tag */
    const char *first_tag = NULL;
    int same_tag = 1;
    c = mxmlGetFirstChild(el);
    while (c) {
        if (mxmlGetType(c) == MXML_TYPE_ELEMENT) {
            const char *t = mxmlGetElement(c);
            if (!first_tag) first_tag = t;
            else if (strcmp(first_tag, t) != 0) { same_tag = 0; break; }
        }
        c = mxmlGetNextSibling(c);
    }

    if (force_array || (same_tag && child_count > 1 && first_tag &&
                        strcmp(first_tag,"item")==0)) {
        cm_node_t *arr = cm_node_new_array(name);
        c = mxmlGetFirstChild(el);
        while (c) {
            if (mxmlGetType(c) == MXML_TYPE_ELEMENT) {
                const char *txt = get_text(c);
                cm_node_t *item = text_to_node(NULL, txt ? txt : "");
                if (item) cm_node_array_add(arr, item);
            }
            c = mxmlGetNextSibling(c);
        }
        return arr;
    }

    /* object */
    cm_node_t *obj = cm_node_new_object(name);
    c = mxmlGetFirstChild(el);
    while (c) {
        if (mxmlGetType(c) == MXML_TYPE_ELEMENT) {
            cm_node_t *child_node = mxml_to_node(c);
            if (child_node) cm_node_object_add(obj, child_node);
        }
        c = mxmlGetNextSibling(c);
    }
    return obj;
}

/* ─── Load ───────────────────────────────────────────────────────── */

static cm_error_t parse_mxml_doc(cm_ctx_t *ctx, mxml_node_t *doc)
{
    cm_ctx_clear(ctx);

    /* find root element */
    mxml_node_t *root_el = mxmlFindElement(doc, doc, NULL, NULL, NULL, MXML_DESCEND_FIRST);
    if (!root_el) return CM_ERR_PARSE;

    /* iterate root's children as top-level keys */
    mxml_node_t *c = mxmlGetFirstChild(root_el);
    while (c) {
        if (mxmlGetType(c) == MXML_TYPE_ELEMENT) {
            cm_node_t *n = mxml_to_node(c);
            if (n) cm_node_object_add(ctx->root, n);
        }
        c = mxmlGetNextSibling(c);
    }

    mxmlDelete(doc);
    ctx->format = CM_FORMAT_XML;
    return CM_OK;
}

cm_error_t cm_xml_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;
#if MXML_MAJOR_VERSION >= 4
    mxml_options_t *opts = mxmlOptionsNew();
    mxmlOptionsSetTypeValue(opts, MXML_TYPE_OPAQUE);
    mxml_node_t *doc = mxmlLoadString(NULL, opts, data);
    mxmlOptionsDelete(opts);
#else
    mxml_node_t *doc = mxmlLoadString(NULL, data, MXML_OPAQUE_CALLBACK);
#endif
    if (!doc) return CM_ERR_PARSE;
    return parse_mxml_doc(ctx, doc);
}

cm_error_t cm_xml_load_file(cm_ctx_t *ctx, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return CM_ERR_IO;
#if MXML_MAJOR_VERSION >= 4
    mxml_options_t *opts = mxmlOptionsNew();
    mxmlOptionsSetTypeValue(opts, MXML_TYPE_OPAQUE);
    mxml_node_t *doc = mxmlLoadFd(NULL, opts, fileno(fp));
    mxmlOptionsDelete(opts);
#else
    mxml_node_t *doc = mxmlLoadFile(NULL, fp, MXML_OPAQUE_CALLBACK);
#endif
    fclose(fp);
    if (!doc) return CM_ERR_PARSE;
    return parse_mxml_doc(ctx, doc);
}

/* ─── Serializer ─────────────────────────────────────────────────── */

static void node_to_mxml(mxml_node_t *parent, cm_node_t *n)
{
    char buf[64];
    const char *tag = (n->key && *n->key) ? n->key : "value";

    switch (n->type) {
        case CM_TYPE_NULL:
            mxmlNewElement(parent, tag);
            break;
        case CM_TYPE_BOOL: {
            mxml_node_t *el = mxmlNewElement(parent, tag);
            mxmlNewOpaque(el, n->value.bval ? "true" : "false");
            break;
        }
        case CM_TYPE_INT: {
            snprintf(buf, sizeof(buf), "%lld", (long long)n->value.ival);
            mxml_node_t *el = mxmlNewElement(parent, tag);
            mxmlNewOpaque(el, buf);
            break;
        }
        case CM_TYPE_FLOAT: {
            snprintf(buf, sizeof(buf), "%g", n->value.fval);
            mxml_node_t *el = mxmlNewElement(parent, tag);
            mxmlNewOpaque(el, buf);
            break;
        }
        case CM_TYPE_STRING: {
            mxml_node_t *el = mxmlNewElement(parent, tag);
            mxmlNewOpaque(el, n->value.sval ? n->value.sval : "");
            break;
        }
        case CM_TYPE_OBJECT: {
            mxml_node_t *el = mxmlNewElement(parent, tag);
            for (size_t i = 0; i < n->value.object.count; i++)
                node_to_mxml(el, n->value.object.children[i]);
            break;
        }
        case CM_TYPE_ARRAY: {
            mxml_node_t *el = mxmlNewElement(parent, tag);
            mxmlElementSetAttr(el, "type", "array");
            for (size_t i = 0; i < n->value.array.count; i++) {
                cm_node_t *item = n->value.array.items[i];
                /* force tag "item" for array elements */
                char *saved_key = item->key;
                item->key = (char *)"item";
                node_to_mxml(el, item);
                item->key = saved_key;
            }
            break;
        }
        default: break;
    }
}

char *cm_xml_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;

    mxml_node_t *doc  = mxmlNewXML("1.0");
    mxml_node_t *root = mxmlNewElement(doc, "config");

    for (size_t i = 0; i < ctx->root->value.object.count; i++)
        node_to_mxml(root, ctx->root->value.object.children[i]);

#if MXML_MAJOR_VERSION >= 4
    mxml_options_t *opts = mxmlOptionsNew();
    mxmlOptionsSetWrapMargin(opts, 0);
    char *str = mxmlSaveAllocString(doc, opts);
    mxmlOptionsDelete(opts);
#else
    char *str = mxmlSaveAllocString(doc, MXML_NO_CALLBACK);
#endif
    mxmlDelete(doc);

    if (out_len && str) *out_len = strlen(str);
    return str;
}

cm_error_t cm_xml_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len = 0;
    char  *str = cm_xml_save_string(ctx, &len);
    if (!str) return CM_ERR_NO_MEMORY;

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(str); return CM_ERR_IO; }
    fwrite(str, 1, len, fp);
    fclose(fp);
    free(str);
    return CM_OK;
}
