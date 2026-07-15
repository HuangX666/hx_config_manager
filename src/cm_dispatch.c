/**
 * cm_dispatch.c  –  Top-level format dispatch
 */

#include "config_manager.h"
#include "cm_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

cm_error_t cm_internal_read_file(const char *path, char **out_data,
                                 size_t *out_size)
{
    FILE *file;
    long file_size;
    size_t bytes_read;
    char *data;

    if (!path || !out_data) return CM_ERR_NULL_PTR;
    *out_data = NULL;
    if (out_size) *out_size = 0;

    file = fopen(path, "rb");
    if (!file) return CM_ERR_IO;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return CM_ERR_IO;
    }
    file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return CM_ERR_IO;
    }
    if ((unsigned long long)file_size >= (unsigned long long)SIZE_MAX) {
        fclose(file);
        return CM_ERR_OVERFLOW;
    }

    data = (char *)malloc((size_t)file_size + 1);
    if (!data) {
        fclose(file);
        return CM_ERR_NO_MEMORY;
    }
    bytes_read = fread(data, 1, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size || ferror(file)) {
        free(data);
        fclose(file);
        return CM_ERR_IO;
    }
    data[bytes_read] = '\0';
    if (fclose(file) != 0) {
        free(data);
        return CM_ERR_IO;
    }

    *out_data = data;
    if (out_size) *out_size = bytes_read;
    return CM_OK;
}

cm_error_t cm_internal_write_file(const char *path, const char *data,
                                  size_t size)
{
    FILE *file;
    size_t bytes_written;
    int close_result;

    if (!path || (!data && size != 0)) return CM_ERR_NULL_PTR;
    file = fopen(path, "wb");
    if (!file) return CM_ERR_IO;
    bytes_written = size ? fwrite(data, 1, size, file) : 0;
    close_result = fclose(file);
    return bytes_written == size && close_result == 0 ? CM_OK : CM_ERR_IO;
}

static void replace_context(cm_ctx_t *destination, cm_ctx_t *source)
{
    cm_node_free(destination->root);
    free(destination->source_path);
    destination->root = source->root;
    destination->format = source->format;
    destination->source_path = source->source_path;
    destination->dirty = 0;
    source->root = NULL;
    source->source_path = NULL;
}

static cm_error_t load_file_direct(cm_ctx_t *ctx, const char *path,
                                   cm_format_t fmt)
{
    switch (fmt) {
        case CM_FORMAT_JSON:       return cm_json_load_file(ctx, path);
        case CM_FORMAT_YAML:       return cm_yaml_load_file(ctx, path);
        case CM_FORMAT_XML:        return cm_xml_load_file(ctx, path);
        case CM_FORMAT_INI:        return cm_ini_load_file(ctx, path);
        case CM_FORMAT_TOML:       return cm_toml_load_file(ctx, path);
        case CM_FORMAT_ENV:        return cm_env_load_file(ctx, path);
        case CM_FORMAT_PROPERTIES: return cm_properties_load_file(ctx, path);
        default:                   return CM_ERR_UNSUPPORTED;
    }
}

static cm_error_t load_string_direct(cm_ctx_t *ctx, const char *data,
                                     cm_format_t fmt)
{
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

/* ─── Load file ──────────────────────────────────────────────────── */

cm_error_t cm_load_file(cm_ctx_t *ctx, const char *path, cm_format_t fmt)
{
    cm_ctx_t *temporary;
    cm_error_t result;
    if (!ctx || !path) return CM_ERR_NULL_PTR;
    if (fmt == CM_FORMAT_AUTO) fmt = cm_detect_format(path);
    if (fmt == CM_FORMAT_AUTO) {
        fprintf(stderr, "[cm] cannot detect format for: %s\n", path);
        return CM_ERR_UNSUPPORTED;
    }

    temporary = cm_ctx_create();
    if (!temporary) return CM_ERR_NO_MEMORY;
    result = load_file_direct(temporary, path, fmt);
    if (result == CM_OK) {
        temporary->source_path = cm_internal_strdup(path);
        if (!temporary->source_path) result = CM_ERR_NO_MEMORY;
    }
    if (result == CM_OK) replace_context(ctx, temporary);
    cm_ctx_destroy(temporary);
    return result;
}

/* ─── Load string ────────────────────────────────────────────────── */

cm_error_t cm_load_string(cm_ctx_t *ctx, const char *data, cm_format_t fmt)
{
    cm_ctx_t *temporary;
    cm_error_t result;
    if (!ctx || !data) return CM_ERR_NULL_PTR;
    if (fmt == CM_FORMAT_AUTO) return CM_ERR_UNSUPPORTED;
    temporary = cm_ctx_create();
    if (!temporary) return CM_ERR_NO_MEMORY;
    result = load_string_direct(temporary, data, fmt);
    if (result == CM_OK) replace_context(ctx, temporary);
    cm_ctx_destroy(temporary);
    return result;
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
