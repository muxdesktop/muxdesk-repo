#define _GNU_SOURCE
#include "apps.h"

#include <string.h>

static MuxApp *app_new(const char *id)
{
    MuxApp *a = g_new0(MuxApp, 1);
    a->id = g_strdup(id);
    a->name = g_strdup(id);
    return a;
}

static void app_free(MuxApp *a)
{
    g_free(a->id);
    g_free(a->name);
    g_free(a->exec);
    g_free(a->icon);
    g_free(a->wm_class);
    g_free(a);
}

MuxApps *mux_apps_new(void)
{
    MuxApps *ma = g_new0(MuxApps, 1);
    ma->apps = g_ptr_array_new_with_free_func((GDestroyNotify)app_free);
    return ma;
}

void mux_apps_free(MuxApps *ma)
{
    if (!ma)
        return;
    g_ptr_array_free(ma->apps, TRUE);
    g_free(ma);
}

int mux_apps_count(MuxApps *ma) { return (int)ma->apps->len; }

MuxApp *mux_apps_get(MuxApps *ma, int i)
{
    return g_ptr_array_index(ma->apps, i);
}

bool mux_apps_add_pinned(MuxApps *ma, char **dirs, const char *id)
{
    for (int d = 0; dirs && dirs[d]; d++) {
        char *path = g_strdup_printf("%s/%s.desktop", dirs[d], id);
        GKeyFile *kf = g_key_file_new();
        gboolean ok = g_key_file_load_from_file(
            kf, path, G_KEY_FILE_NONE, NULL);
        g_free(path);
        if (!ok) {
            g_key_file_free(kf);
            continue;
        }
        if (g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", NULL) ||
            g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL)) {
            g_key_file_free(kf);
            return false;
        }
        MuxApp *a = app_new(id);
        char *v;
        if ((v = g_key_file_get_string(kf, "Desktop Entry", "Name", NULL))) {
            g_free(a->name);
            a->name = v;
        }
        a->exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);
        a->icon = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
        a->wm_class = g_key_file_get_string(kf, "Desktop Entry",
                                            "StartupWMClass", NULL);
        a->pinned = true;
        g_key_file_free(kf);
        if (!a->exec) { /* a launcher without Exec is useless */
            app_free(a);
            return false;
        }
        g_ptr_array_add(ma->apps, a);
        return true;
    }
    return false;
}

static bool ci_equal(const char *a, const char *b)
{
    return a && b && g_ascii_strcasecmp(a, b) == 0;
}

bool mux_app_matches(const MuxApp *app, const char *app_id)
{
    if (!app_id || !*app_id)
        return false;
    if (ci_equal(app->id, app_id) || ci_equal(app->wm_class, app_id))
        return true;
    const char *dot = strrchr(app_id, '.');
    if (dot && ci_equal(app->id, dot + 1))
        return true;
    char *with_ext = g_strdup_printf("%s.desktop", app->id);
    bool m = ci_equal(with_ext, app_id);
    g_free(with_ext);
    return m;
}

MuxApp *mux_apps_note_running(MuxApps *ma, const char *app_id, bool active)
{
    for (guint i = 0; i < ma->apps->len; i++) {
        MuxApp *a = g_ptr_array_index(ma->apps, i);
        if (mux_app_matches(a, app_id)) {
            a->running = true;
            if (active)
                a->active = true;
            return a;
        }
    }
    MuxApp *a = app_new(app_id);
    a->running = true;
    a->active = active;
    g_ptr_array_add(ma->apps, a);
    return a;
}

void mux_apps_begin_sync(MuxApps *ma)
{
    for (guint i = 0; i < ma->apps->len; i++) {
        MuxApp *a = g_ptr_array_index(ma->apps, i);
        a->running = false;
        a->active = false;
    }
}

void mux_apps_prune(MuxApps *ma)
{
    for (gint i = (gint)ma->apps->len - 1; i >= 0; i--) {
        MuxApp *a = g_ptr_array_index(ma->apps, i);
        if (!a->pinned && !a->running)
            g_ptr_array_remove_index(ma->apps, i);
    }
}

char *mux_app_launch_command(const MuxApp *app)
{
    if (!app->exec)
        return NULL;
    GString *out = g_string_new("");
    const char *s = app->exec;
    bool last_was_space = true; /* also trims leading spaces */
    for (; *s; s++) {
        if (*s == '%' && s[1]) {
            char code = s[1];
            if (code == '%') {
                g_string_append_c(out, '%');
                last_was_space = false;
            }
            /* %f %F %u %U %d %D %n %N %i %c %k %v %m: dropped.
             * Dropping %i loses "--icon x" expansion; acceptable —
             * none of the MuxLike launchers use it. */
            s++;
            continue;
        }
        if (*s == ' ' || *s == '\t') {
            if (!last_was_space)
                g_string_append_c(out, ' ');
            last_was_space = true;
            continue;
        }
        g_string_append_c(out, *s);
        last_was_space = false;
    }
    while (out->len > 0 && out->str[out->len - 1] == ' ')
        g_string_truncate(out, out->len - 1);
    return g_string_free(out, FALSE);
}
