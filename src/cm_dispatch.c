/**
 * cm_dispatch.c  –  Top-level format dispatch
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Load file ──────────────────────────────────────────────────── */

cm_error_t cm_load_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt)
{
    if (!ctx || !path) return CM_ERR_NULL_PTR;
    if (fmt == CM_FORMAT_AUTO) fmt = cm_detect_format(path);

    switch (fmt) {
        case CM_FORMAT_JSON:       return cm_json_load_file(ctx, path);
        case CM_FORMAT_YAML:       return cm_yaml_load_file(ctx, path);
        case CM_FORMAT_XML:        return cm_xml_load_file(ctx, path);
        case CM_FORMAT_INI:        return cm_ini_load_file(ctx, path);
        case CM_FORMAT_TOML:       return cm_toml_load_file(ctx, path);
        case CM_FORMAT_ENV:        return cm_env_load_file(ctx, path);
        case CM_FORMAT_PROPERTIES: return cm_properties_load_file(ctx, path);
        default:
            fprintf(stderr, "[cm] cannot detect format for: %s\n", path);
            return CM_ERR_UNSUPPORTED;
    }
}

/* ─── Load string ────────────────────────────────────────────────── */

cm_error_t cm_load_string(cm_ctx_t *ctx, const char *data, cm_format_t fmt)
{
    if (!ctx || !data) return CM_ERR_NULL_PTR;
    switch (fmt) {
        case CM_FORMAT_JSON:       return cm_json_load_string(ctx, data);
        case CM_FORMAT_YAML:       return cm_yaml_load_string(ctx, data);
        case CM_FORMAT_XML:        return cm_xml_load_string(ctx, data);
        case CM_FORMAT_INI:        return cm_ini_load_string(ctx, data);
        case CM_FORMAT_TOML:       return cm_toml_load_string(ctx, data);
        case CM_FORMAT_ENV:        return cm_env_load_string(ctx, data);
        case CM_FORMAT_PROPERTIES: return cm_properties_load_string(ctx, data);
        default:                   return CM_ERR_UNSUPPORTED;
    }
}

/* ─── Save file ──────────────────────────────────────────────────── */

cm_error_t cm_save_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt)
{
    if (!ctx || !path) return CM_ERR_NULL_PTR;
    if (fmt == CM_FORMAT_AUTO) fmt = cm_detect_format(path);
    if (fmt == CM_FORMAT_AUTO) fmt = ctx->format;
    if (fmt == CM_FORMAT_AUTO) fmt = CM_FORMAT_JSON; /* last resort */

    switch (fmt) {
        case CM_FORMAT_JSON:       return cm_json_save_file(ctx, path);
        case CM_FORMAT_YAML:       return cm_yaml_save_file(ctx, path);
        case CM_FORMAT_XML:        return cm_xml_save_file(ctx, path);
        case CM_FORMAT_INI:        return cm_ini_save_file(ctx, path);
        case CM_FORMAT_TOML:       return cm_toml_save_file(ctx, path);
        case CM_FORMAT_ENV:        return cm_env_save_file(ctx, path);
        case CM_FORMAT_PROPERTIES: return cm_properties_save_file(ctx, path);
        default:                   return CM_ERR_UNSUPPORTED;
    }
}

/* ─── Save string ────────────────────────────────────────────────── */

char *cm_save_string(cm_ctx_t *ctx, cm_format_t fmt, size_t *out_len)
{
    if (!ctx) return NULL;
    if (fmt == CM_FORMAT_AUTO) fmt = ctx->format;
    if (fmt == CM_FORMAT_AUTO) fmt = CM_FORMAT_JSON;

    switch (fmt) {
        case CM_FORMAT_JSON:       return cm_json_save_string(ctx, out_len);
        case CM_FORMAT_YAML:       return cm_yaml_save_string(ctx, out_len);
        case CM_FORMAT_XML:        return cm_xml_save_string(ctx, out_len);
        case CM_FORMAT_INI:        return cm_ini_save_string(ctx, out_len);
        case CM_FORMAT_TOML:       return cm_toml_save_string(ctx, out_len);
        case CM_FORMAT_ENV:        return cm_env_save_string(ctx, out_len);
        case CM_FORMAT_PROPERTIES: return cm_properties_save_string(ctx, out_len);
        default:                   return NULL;
    }
}
