/**
 * cm_internal.h  –  Internal declarations shared between source files
 */
#ifndef CM_INTERNAL_H
#define CM_INTERNAL_H

#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Format-specific load/save ─────────────────────────────────── */

/* JSON */
cm_error_t cm_json_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_json_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_json_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_json_save_file(cm_ctx_t *ctx, const char *path);

/* YAML */
cm_error_t cm_yaml_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_yaml_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_yaml_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_yaml_save_file(cm_ctx_t *ctx, const char *path);

/* XML */
cm_error_t cm_xml_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_xml_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_xml_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_xml_save_file(cm_ctx_t *ctx, const char *path);

/* INI */
cm_error_t cm_ini_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_ini_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_ini_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_ini_save_file(cm_ctx_t *ctx, const char *path);

/* TOML */
cm_error_t cm_toml_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_toml_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_toml_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_toml_save_file(cm_ctx_t *ctx, const char *path);

/* ENV */
cm_error_t cm_env_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_env_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_env_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_env_save_file(cm_ctx_t *ctx, const char *path);

/* Properties */
cm_error_t cm_properties_load_string(cm_ctx_t *ctx, const char *data);
cm_error_t cm_properties_load_file(cm_ctx_t *ctx, const char *path);
char      *cm_properties_save_string(cm_ctx_t *ctx, size_t *out_len);
cm_error_t cm_properties_save_file(cm_ctx_t *ctx, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CM_INTERNAL_H */
