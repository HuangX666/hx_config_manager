/**
 * config_watcher.c  –  Hot-reload config file when it changes on disk.
 *
 * Cross-platform strategy:
 *   - POSIX : stat(2) mtime polling
 *   - Windows: GetFileTime polling
 *
 * Usage:  config_watcher <config_file> [interval_ms]
 *
 * The watcher runs for 30 s then exits. In a real server you'd run this
 * in a background thread and signal the worker threads to re-read the
 * config atomically.
 */
#include "config_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─── Platform abstraction ───────────────────────────────────────── */

#ifdef _WIN32
#  include <windows.h>
   typedef FILETIME cm_mtime_t;

   static int get_mtime(const char *path, cm_mtime_t *out)
   {
       WIN32_FILE_ATTRIBUTE_DATA fa;
       if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) return -1;
       *out = fa.ftLastWriteTime;
       return 0;
   }

   static int mtime_changed(const cm_mtime_t *a, const cm_mtime_t *b)
   {
       return a->dwLowDateTime  != b->dwLowDateTime ||
              a->dwHighDateTime != b->dwHighDateTime;
   }

   static void sleep_ms(int ms) { Sleep((DWORD)ms); }

#else
#  include <sys/stat.h>
#  include <unistd.h>
   typedef struct timespec cm_mtime_t;

   static int get_mtime(const char *path, cm_mtime_t *out)
   {
       struct stat st;
       if (stat(path, &st) != 0) return -1;
#  ifdef __APPLE__
       *out = st.st_mtimespec;
#  else
       *out = st.st_mtim;
#  endif
       return 0;
   }

   static int mtime_changed(const cm_mtime_t *a, const cm_mtime_t *b)
   {
       return a->tv_sec != b->tv_sec || a->tv_nsec != b->tv_nsec;
   }

   static void sleep_ms(int ms)
   {
       struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
       nanosleep(&ts, NULL);
   }
#endif

/* ─── Config watcher state ───────────────────────────────────────── */

typedef struct {
    cm_ctx_t    *cfg;
    char         path[512];
    cm_format_t  fmt;
    cm_mtime_t   last_mtime;
    int          load_count;
} cm_watcher_t;

static int watcher_init(cm_watcher_t *w, const char *path, cm_format_t fmt)
{
    memset(w, 0, sizeof(*w));
    snprintf(w->path, sizeof(w->path), "%s", path);
    w->fmt = fmt;
    w->cfg = cm_ctx_create();
    if (!w->cfg) return -1;

    /* initial load */
    cm_error_t err = cm_load_file(w->cfg, path, fmt);
    if (err != CM_OK) {
        fprintf(stderr, "[watcher] initial load failed: %s\n", cm_error_str(err));
        /* non-fatal: file may not exist yet */
    } else {
        w->load_count++;
        get_mtime(path, &w->last_mtime);
        printf("[watcher] Loaded '%s' (format=%s)\n", path, cm_format_str(fmt));
    }
    return 0;
}

/* Returns 1 if config was reloaded, 0 if unchanged, -1 on error */
static int watcher_poll(cm_watcher_t *w)
{
    cm_mtime_t cur;
    if (get_mtime(w->path, &cur) != 0) return -1;
    if (!mtime_changed(&cur, &w->last_mtime)) return 0;

    /* file changed – reload into a new context, then swap */
    cm_ctx_t *new_cfg = cm_ctx_create();
    cm_error_t err = cm_load_file(new_cfg, w->path, w->fmt);
    if (err != CM_OK) {
        fprintf(stderr, "[watcher] reload failed: %s\n", cm_error_str(err));
        cm_ctx_destroy(new_cfg);
        return -1;
    }

    /* atomic-ish swap (single-threaded here; in MT use a mutex) */
    cm_ctx_destroy(w->cfg);
    w->cfg        = new_cfg;
    w->last_mtime = cur;
    w->load_count++;
    return 1;
}

static void watcher_free(cm_watcher_t *w)
{
    cm_ctx_destroy(w->cfg);
}

/* ─── Demo: create a temp JSON config and watch it ──────────────── */

#ifdef _WIN32
#  define TEMP_DIR "C:\\Temp\\"
#else
#  define TEMP_DIR "/tmp/"
#endif

static const char *CFG_V1 =
    "{\"app\":{\"name\":\"WatchedApp\",\"port\":8080,\"version\":1}}";
static const char *CFG_V2 =
    "{\"app\":{\"name\":\"WatchedApp\",\"port\":9090,\"version\":2,\"debug\":true}}";

static void write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror("fopen"); return; }
    fwrite(content, 1, strlen(content), fp);
    fclose(fp);
}

int main(int argc, char **argv)
{
    char   path[512];
    int    interval_ms = 500;
    cm_format_t fmt = CM_FORMAT_AUTO;

    if (argc >= 2) {
        snprintf(path, sizeof(path), "%s", argv[1]);
        if (argc >= 3) interval_ms = atoi(argv[2]);
    } else {
        /* demo mode: use a temp file we create ourselves */
        snprintf(path, sizeof(path), "%sconfig_watch_demo.json", TEMP_DIR);
        write_file(path, CFG_V1);
        fmt = CM_FORMAT_JSON;
    }

    printf("=== config_watcher demo ===\n");
    printf("  Watching : %s\n", path);
    printf("  Interval : %d ms\n\n", interval_ms);

    cm_watcher_t w;
    if (watcher_init(&w, path, fmt) != 0) return 1;

    /* demo: after 2 s, overwrite file with v2, then watch detect it */
    int demo_updated = 0;
    int iterations   = 0;
    int max_iter     = 60; /* ~30 s at 500 ms */

    while (iterations++ < max_iter) {
        int changed = watcher_poll(&w);

        if (changed == 1) {
            printf("[%lds] Config reloaded (load #%d)\n",
                   (long)time(NULL), w.load_count);

            /* print selected keys */
            printf("  app.port    = %lld\n",
                   (long long)cm_get_int_or(w.cfg, "app.port", -1));
            printf("  app.version = %lld\n",
                   (long long)cm_get_int_or(w.cfg, "app.version", -1));
            int dbg = cm_get_bool_or(w.cfg, "app.debug", 0);
            printf("  app.debug   = %s\n", dbg ? "true" : "false");
        }

        /* demo mode: auto-update file after 2 s */
        if (argc < 2 && !demo_updated && iterations == 4) {
            printf("\n[demo] Writing updated config (v2)...\n");
            write_file(path, CFG_V2);
            demo_updated = 1;
        }

        /* demo mode: stop after seeing the reload */
        if (argc < 2 && w.load_count >= 2) {
            printf("\n[demo] Detected config change. Exiting.\n");
            break;
        }

        sleep_ms(interval_ms);
    }

    /* cleanup temp file in demo mode */
    if (argc < 2) {
#ifdef _WIN32
        DeleteFileA(path);
#else
        unlink(path);
#endif
    }

    watcher_free(&w);
    printf("config_watcher done. Total reloads: %d\n", w.load_count);
    return 0;
}
