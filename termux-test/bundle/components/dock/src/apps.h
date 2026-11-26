/* mux-dock — the dock's app list: pinned .desktop entries merged with
 * running apps reported by the compositor (foreign-toplevel).
 * Parsing/matching is display-independent and unit-tested. */
#ifndef MUX_APPS_H
#define MUX_APPS_H

#include <glib.h>
#include <stdbool.h>

typedef struct {
    char *id;       /* desktop-file id, e.g. "mux-files" (no .desktop) */
    char *name;     /* Name= */
    char *exec;     /* Exec=, raw */
    char *icon;     /* Icon= (theme name or path), may be NULL */
    char *wm_class; /* StartupWMClass=, may be NULL */
    bool pinned;
    bool running;
    bool active;
} MuxApp;

typedef struct {
    GPtrArray *apps; /* MuxApp* */
} MuxApps;

MuxApps *mux_apps_new(void);
void mux_apps_free(MuxApps *ma);
int mux_apps_count(MuxApps *ma);
MuxApp *mux_apps_get(MuxApps *ma, int i);

/* Load one pinned entry by desktop id, searching dirs (NULL-terminated
 * array of directory paths containing .desktop files) in order.
 * Missing files are skipped silently — a pinned app that is not
 * installed must not leave a dead icon in the dock. */
bool mux_apps_add_pinned(MuxApps *ma, char **dirs, const char *id);

/* Does this entry correspond to a compositor app_id?
 * True when: equal (case-insensitive), StartupWMClass matches, the
 * app_id's last dot-component equals the id, or id + ".desktop" forms
 * match. This mirrors how labwc/wlroots app_ids relate to desktop
 * files in practice. */
bool mux_app_matches(const MuxApp *app, const char *app_id);

/* Find entry for app_id or create an unpinned one (name = app_id).
 * Sets running/active. Used to surface running-but-unpinned apps. */
MuxApp *mux_apps_note_running(MuxApps *ma, const char *app_id,
                              bool active);
/* Clear all running/active flags (before a fresh sync). Pinned stay,
 * unpinned entries with running=false are dropped by prune. */
void mux_apps_begin_sync(MuxApps *ma);
void mux_apps_prune(MuxApps *ma);

/* Exec= with field codes (%f %F %u %U %i %c %k %%) removed, ready for
 * g_spawn_command_line_async. Caller frees. */
char *mux_app_launch_command(const MuxApp *app);

#endif /* MUX_APPS_H */
