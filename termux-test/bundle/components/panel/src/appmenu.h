/* mux-panel — global menu (macOS-style menu bar) over D-Bus.
 *
 * Two halves, both on the session bus:
 *  1. We own com.canonical.AppMenu.Registrar and collect RegisterWindow
 *     calls from apps (the Unity/appmenu contract; our own apps follow
 *     the MuxLike contract: menu model at /com/muxdesk/menu/<short>,
 *     action group at /com/muxdesk/actions/<short>, window_key = a
 *     stable hash of the app id, service = their unique bus name).
 *  2. For the focused app (foreign-toplevel app_id -> GetNameOwner ->
 *     the registration whose sender is that owner) we subscribe to its
 *     org.gtk.Menus model and expose the top-level titles + dropdown
 *     items to the panel for drawing. Clicks go back as
 *     org.gtk.Actions.Activate on the app/window action path.
 *
 * Everything is best-effort: no bus, no registrar name, no matching
 * registration or a dead app must never take the panel down — the
 * panel simply falls back to showing just the app name.
 */
#ifndef MUX_APPMENU_H
#define MUX_APPMENU_H

#include <gio/gio.h>
#include <glib.h>
#include <stdbool.h>

typedef struct {
    char *label;      /* may be NULL for a separator-ish item */
    char *action;     /* detailed name, e.g. "app.quit"; may be NULL */
    GVariant *target; /* item target parameter, owned; may be NULL */
    bool enabled;     /* from org.gtk.Actions DescribeAll (default true) */
    bool has_children; /* :submenu/:section link — shown, not activated */
} AppMenuItem;

typedef struct {
    char *label;     /* top-level title, e.g. "File" */
    GPtrArray *items; /* AppMenuItem* — the dropdown contents */
} AppMenuTitle;

typedef struct AppMenu AppMenu;

/* on_change fires (from the default main context) whenever the title
 * model for the focused app appears, changes or disappears. */
AppMenu *mux_appmenu_new(GDBusConnection *bus,
                         void (*on_change)(void *data), void *data);
/* Own the Registrar name + register the object. False on failure —
 * the panel keeps running without a global menu. */
bool mux_appmenu_start_registrar(AppMenu *am);
/* The focused window's app_id changed (or NULL for none). Resolution
 * is synchronous but bounded (short timeouts, local bus) and fully
 * failure-tolerant. */
void mux_appmenu_set_focus(AppMenu *am, const char *app_id);
bool mux_appmenu_has_menu(AppMenu *am);
int mux_appmenu_title_count(AppMenu *am);
const AppMenuTitle *mux_appmenu_title(AppMenu *am, int index);
/* Fire org.gtk.Actions.Activate for a dropdown item (async, no reply
 * waited on). Safe to call with any item from the current model. */
void mux_appmenu_activate(AppMenu *am, const AppMenuItem *item);
void mux_appmenu_free(AppMenu *am);

#endif /* MUX_APPMENU_H */
