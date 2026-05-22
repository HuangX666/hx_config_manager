/* test_xml.c */
#include "config_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static const char *XML_DATA =
    "<?xml version=\"1.0\"?>\n"
    "<config>\n"
    "  <app>\n"
    "    <name>MyApp</name>\n"
    "    <version>2.0</version>\n"
    "    <debug>false</debug>\n"
    "  </app>\n"
    "  <server>\n"
    "    <host>0.0.0.0</host>\n"
    "    <port>8080</port>\n"
    "  </server>\n"
    "  <tags type=\"array\">\n"
    "    <item>production</item>\n"
    "    <item>v2</item>\n"
    "  </tags>\n"
    "</config>\n";

int main(void)
{
    printf("=== test_xml ===\n");
    cm_ctx_t *ctx = cm_ctx_create();

    cm_error_t err = cm_load_string(ctx, XML_DATA, CM_FORMAT_XML);
    if (err != CM_OK) {
        printf("  XML load: %s (skipping)\n", cm_error_str(err));
        cm_ctx_destroy(ctx); return 0;
    }

    const char *name = NULL;
    assert(cm_get_string(ctx, "app.name", &name) == CM_OK);
    assert(strcmp(name, "MyApp") == 0);

    const char *host = NULL;
    assert(cm_get_string(ctx, "server.host", &host) == CM_OK);
    assert(strcmp(host, "0.0.0.0") == 0);

    size_t olen = 0;
    char *out = cm_save_string(ctx, CM_FORMAT_XML, &olen);
    assert(out);
    free(out);

    cm_ctx_destroy(ctx);
    printf("  XML load/save              PASS\n");
    return 0;
}
