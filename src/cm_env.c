/**
 * cm_env.c  –  .env / .properties load/save (pure C, zero dependencies)
 *
 * .env format:
 *   # comment
 *   KEY=value
 *   KEY="quoted value"
 *   export KEY=value        (shell export prefix ignored)
 *
 * .properties format (Java style):
 *   # or ! comment
 *   key = value
 *   key: value
 *   key value               (whitespace separator)
 *   key.sub = value         (dot-notation → nested objects)
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── Shared helpers ─────────────────────────────────────────────── */

static char *ltrim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)*(e-1))) e--;
    *e = '\0';
}

/* Unquote a value: remove surrounding ' or " and handle basic escapes */
static char *unquote(const char *val, char *buf, size_t blen)
{
    if (!val || !*val) { buf[0]='\0'; return buf; }
    char q = val[0];
    size_t len = strlen(val);
    if ((q == '"' || q == '\'') && len >= 2 && val[len-1] == q) {
        val++;
        len -= 2;
        size_t j = 0;
        for (size_t i = 0; i < len && j < blen-1; i++) {
            if (val[i] == '\\' && i+1 < len) {
                i++;
                switch (val[i]) {
                    case 'n':  buf[j++]='\n'; break;
                    case 't':  buf[j++]='\t'; break;
                    case 'r':  buf[j++]='\r'; break;
                    default:   buf[j++]=val[i]; break;
                }
            } else {
                buf[j++] = val[i];
            }
        }
        buf[j] = '\0';
        return buf;
    }
    snprintf(buf, blen, "%s", val);
    return buf;
}

/* Auto-type a string value */
static cm_error_t set_auto(cm_ctx_t *ctx, const char *key, const char *raw)
{
    if (!raw || !*raw) return cm_set_string(ctx, key, raw);

    if (strcmp(raw,"true")==0 || strcmp(raw,"yes")==0 || strcmp(raw,"TRUE")==0)
        return cm_set_bool(ctx, key, 1);
    if (strcmp(raw,"false")==0|| strcmp(raw,"no")==0  || strcmp(raw,"FALSE")==0)
        return cm_set_bool(ctx, key, 0);

    char *ep = NULL;
    int64_t iv = strtoll(raw, &ep, 10);
    if (ep && *ep=='\0' && ep!=raw) return cm_set_int(ctx, key, iv);

    double dv = strtod(raw, &ep);
    if (ep && *ep=='\0' && ep!=raw) return cm_set_float(ctx, key, dv);

    return cm_set_string(ctx, key, raw);
}

/* ─── .env loader ────────────────────────────────────────────────── */

static cm_error_t parse_env_data(cm_ctx_t *ctx, const char *data)
{
    char *copy = strdup(data);
    if (!copy) return CM_ERR_NO_MEMORY;

    char *line = copy;
    while (*line) {
        /* find end of line */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *p = ltrim(line);
        rtrim(p);

        /* skip blank / comment */
        if (*p && *p != '#') {
            /* strip "export " prefix */
            if (strncmp(p, "export ", 7) == 0) p += 7;
            p = ltrim(p);

            /* split on first '=' */
            char *eq = strchr(p, '=');
            if (eq) {
                *eq = '\0';
                char *key = p; rtrim(key);
                char *val = eq + 1;
                /* handle inline comment after unquoted value */
                char vbuf[4096];
                unquote(val, vbuf, sizeof(vbuf));
                set_auto(ctx, key, vbuf);
            }
        }

        if (!nl) break;
        line = nl + 1;
    }
    free(copy);
    return CM_OK;
}

cm_error_t cm_env_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;
    cm_ctx_clear(ctx);
    cm_error_t err = parse_env_data(ctx, data);
    if (err == CM_OK) ctx->format = CM_FORMAT_ENV;
    return err;
}

cm_error_t cm_env_load_file(cm_ctx_t *ctx, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return CM_ERR_IO;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return CM_ERR_NO_MEMORY; }
    fread(buf, 1, (size_t)sz, fp); buf[sz]='\0'; fclose(fp);
    cm_error_t err = cm_env_load_string(ctx, buf);
    free(buf);
    return err;
}

/* ─── .env save ──────────────────────────────────────────────────── */

typedef struct { char *buf; size_t len; size_t cap; } str_buf_t;
static void sb_append(str_buf_t *b, const char *s)
{
    size_t sl = strlen(s);
    if (b->len + sl + 1 > b->cap) {
        size_t nc = b->cap ? b->cap*2 : 4096;
        while (nc < b->len+sl+1) nc*=2;
        char *p = (char*)realloc(b->buf, nc);
        if (!p) return;
        b->buf=p; b->cap=nc;
    }
    memcpy(b->buf+b->len, s, sl);
    b->len+=sl;
    b->buf[b->len]='\0';
}

static void env_walk(const char *path, cm_node_t *node, void *ud)
{
    str_buf_t *out = (str_buf_t *)ud;
    char val[1024] = {0};
    int needs_quotes = 0;

    switch (node->type) {
        case CM_TYPE_NULL:   strcpy(val, ""); break;
        case CM_TYPE_BOOL:   strcpy(val, node->value.bval?"true":"false"); break;
        case CM_TYPE_INT:    snprintf(val,sizeof(val),"%lld",(long long)node->value.ival); break;
        case CM_TYPE_FLOAT:  snprintf(val,sizeof(val),"%g",node->value.fval); break;
        case CM_TYPE_STRING:
            snprintf(val,sizeof(val),"%s",node->value.sval?node->value.sval:"");
            /* quote if spaces or special chars */
            if (strchr(val,' ')||strchr(val,'\t')||strchr(val,'#'))
                needs_quotes=1;
            break;
        default: return;
    }

    /* convert dot-path to underscore for env keys */
    char env_key[512];
    snprintf(env_key, sizeof(env_key), "%s", path);
    for (char *p = env_key; *p; p++) {
        if (*p == '.' || *p == '[' || *p == ']') *p = '_';
        else *p = toupper((unsigned char)*p);
    }

    sb_append(out, env_key);
    sb_append(out, "=");
    if (needs_quotes) {
        sb_append(out, "\"");
        sb_append(out, val);
        sb_append(out, "\"");
    } else {
        sb_append(out, val);
    }
    sb_append(out, "\n");
}

char *cm_env_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;
    str_buf_t out = {NULL,0,0};
    cm_walk(ctx, env_walk, &out);
    if (out_len) *out_len = out.len;
    return out.buf ? out.buf : strdup("");
}

cm_error_t cm_env_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len=0;
    char *str = cm_env_save_string(ctx,&len);
    if (!str) return CM_ERR_NO_MEMORY;
    FILE *fp = fopen(path,"wb");
    if (!fp) { free(str); return CM_ERR_IO; }
    fwrite(str,1,len,fp); fclose(fp); free(str);
    return CM_OK;
}

/* ─── .properties loader ─────────────────────────────────────────── */

static cm_error_t parse_properties_data(cm_ctx_t *ctx, const char *data)
{
    char *copy = strdup(data);
    if (!copy) return CM_ERR_NO_MEMORY;

    char *line = copy;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *p = ltrim(line);

        /* handle line continuation */
        while (p[strlen(p)-1] == '\\' && nl) {
            p[strlen(p)-1] = '\0';
            /* TODO: accumulate – simple approach for now */
        }

        rtrim(p);
        if (*p && *p != '#' && *p != '!') {
            /* find separator: '=', ':', or first whitespace */
            char *sep = strpbrk(p, "=: \t");
            if (sep) {
                char sep_char = *sep;
                *sep = '\0';
                char *key = p; rtrim(key);
                char *val = sep + 1;
                if (sep_char == ' ' || sep_char == '\t') {
                    /* skip optional '=' or ':' after whitespace */
                    val = ltrim(val);
                    if (*val == '=' || *val == ':') val++;
                }
                val = ltrim(val);
                rtrim(val);

                /* convert key dots to nested path */
                set_auto(ctx, key, val);
            }
        }

        if (!nl) break;
        line = nl + 1;
    }
    free(copy);
    return CM_OK;
}

cm_error_t cm_properties_load_string(cm_ctx_t *ctx, const char *data)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;
    cm_ctx_clear(ctx);
    cm_error_t err = parse_properties_data(ctx, data);
    if (err == CM_OK) ctx->format = CM_FORMAT_PROPERTIES;
    return err;
}

cm_error_t cm_properties_load_file(cm_ctx_t *ctx, const char *path)
{
    FILE *fp = fopen(path,"rb");
    if (!fp) return CM_ERR_IO;
    fseek(fp,0,SEEK_END); long sz=ftell(fp); rewind(fp);
    char *buf=(char*)malloc((size_t)sz+1);
    if (!buf) { fclose(fp); return CM_ERR_NO_MEMORY; }
    fread(buf,1,(size_t)sz,fp); buf[sz]='\0'; fclose(fp);
    cm_error_t err = cm_properties_load_string(ctx, buf);
    free(buf);
    return err;
}

static void props_walk(const char *path, cm_node_t *node, void *ud)
{
    str_buf_t *out = (str_buf_t *)ud;
    char val[1024] = {0};
    switch (node->type) {
        case CM_TYPE_NULL:   strcpy(val,""); break;
        case CM_TYPE_BOOL:   strcpy(val, node->value.bval?"true":"false"); break;
        case CM_TYPE_INT:    snprintf(val,sizeof(val),"%lld",(long long)node->value.ival); break;
        case CM_TYPE_FLOAT:  snprintf(val,sizeof(val),"%g",node->value.fval); break;
        case CM_TYPE_STRING: snprintf(val,sizeof(val),"%s",node->value.sval?node->value.sval:""); break;
        default: return;
    }
    sb_append(out, path);
    sb_append(out, " = ");
    sb_append(out, val);
    sb_append(out, "\n");
}

char *cm_properties_save_string(cm_ctx_t *ctx, size_t *out_len)
{
    if (!ctx) return NULL;
    str_buf_t out = {NULL,0,0};
    sb_append(&out, "# Generated by config_manager\n");
    cm_walk(ctx, props_walk, &out);
    if (out_len) *out_len = out.len;
    return out.buf ? out.buf : strdup("");
}

cm_error_t cm_properties_save_file(cm_ctx_t *ctx, const char *path)
{
    size_t len=0;
    char *str = cm_properties_save_string(ctx,&len);
    if (!str) return CM_ERR_NO_MEMORY;
    FILE *fp = fopen(path,"wb");
    if (!fp) { free(str); return CM_ERR_IO; }
    fwrite(str,1,len,fp); fclose(fp); free(str);
    return CM_OK;
}
