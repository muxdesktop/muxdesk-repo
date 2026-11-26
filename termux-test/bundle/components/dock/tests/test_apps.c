/* Unit tests for .desktop parsing, app_id matching, Exec cleaning. */
#include "../src/apps.h"
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

static char *write_desktop(const char *dir, const char *id,
                           const char *contents)
{
    char *path = g_strdup_printf("%s/%s.desktop", dir, id);
    g_file_set_contents(path, contents, -1, NULL);
    return path;
}

int main(void)
{
    char *tmp = g_dir_make_tmp("mux-dock-test-XXXXXX", NULL);
    assert(tmp);
    write_desktop(tmp, "mux-files",
                  "[Desktop Entry]\n"
                  "Name=Mux Files\n"
                  "Exec=mux-files %U\n"
                  "Icon=mux-files\n"
                  "StartupWMClass=MuxFiles\n"
                  "Type=Application\n");
    write_desktop(tmp, "mux-hidden",
                  "[Desktop Entry]\nName=Hidden\nExec=x\nNoDisplay=true\n");
    char *dirs[] = { tmp, NULL };

    MuxApps *ma = mux_apps_new();
    assert(mux_apps_add_pinned(ma, dirs, "mux-files"));
    assert(!mux_apps_add_pinned(ma, dirs, "mux-not-installed"));
    assert(!mux_apps_add_pinned(ma, dirs, "mux-hidden"));
    assert(mux_apps_count(ma) == 1);

    MuxApp *files = mux_apps_get(ma, 0);
    assert(strcmp(files->name, "Mux Files") == 0);
    assert(files->pinned);

    /* matching rules */
    assert(mux_app_matches(files, "mux-files"));
    assert(mux_app_matches(files, "MuxFiles"));       /* WM class */
    assert(mux_app_matches(files, "org.mux.mux-files")); /* last component */
    assert(!mux_app_matches(files, "mux-browser"));
    assert(!mux_app_matches(files, ""));

    /* Exec field codes stripped */
    char *cmd = mux_app_launch_command(files);
    assert(strcmp(cmd, "mux-files") == 0);
    g_free(cmd);
    files->exec = g_strdup("env FOO=1 app --flag %f %% %U");
    /* note: literal %% collapses to %, rest dropped */
    cmd = mux_app_launch_command(files);
    assert(strcmp(cmd, "env FOO=1 app --flag %") == 0);
    g_free(cmd);

    /* running sync: unpinned app appears, then prunes when closed */
    mux_apps_begin_sync(ma);
    mux_apps_note_running(ma, "mux-files", true);
    mux_apps_note_running(ma, "some-random-app", false);
    mux_apps_prune(ma);
    assert(mux_apps_count(ma) == 2);
    assert(files->running && files->active);
    mux_apps_begin_sync(ma);
    mux_apps_prune(ma);
    assert(mux_apps_count(ma) == 1); /* random app gone, pinned stays */
    assert(!files->running);

    mux_apps_free(ma);
    puts("test-apps: OK");
    return 0;
}
