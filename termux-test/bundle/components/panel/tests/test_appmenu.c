/* End-to-end test of the global menu over a real session bus.
 *
 * Run under dbus-run-session (see run-appmenu-test.sh / make target):
 *   - starts tests/fake-menu-app.py --headless (contract-conformant:
 *     exports menu+actions, RegisterWindow (u,s,o,o))
 *   - mux_appmenu owns the Registrar name, resolves focus by app_id,
 *     subscribes to org.gtk.Menus and parses titles/items/enabled
 *   - GetMenuForWindow is checked from a second connection
 *   - activating an item must reach the fake app ("ACTIVATED ...")
 *   - killing the fake app must clear the menu (NameOwnerChanged)
 */
#define _GNU_SOURCE
#include "../src/appmenu.h"
#include <assert.h>
#include <gio/gio.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FAKE_APP_ID "com.muxdesk.fakemenu"
#define FAKE_WINDOW_KEY 784389159u /* crc32(FAKE_APP_ID) */

static AppMenu *am;
static GString *fake_output;
static GPid fake_pid;

static void on_change(void *data) { (void)data; }

static gboolean on_fake_out(GIOChannel *ch, GIOCondition cond, void *data)
{
    (void)cond; (void)data;
    char buf[512];
    gsize n = 0;
    if (g_io_channel_read_chars(ch, buf, sizeof(buf) - 1, &n, NULL) ==
        G_IO_STATUS_NORMAL) {
        buf[n] = '\0';
        g_string_append(fake_output, buf);
    }
    return TRUE;
}

static bool pump_until(bool (*cond)(void *), void *data, int timeout_ms)
{
    gint64 end = g_get_monotonic_time() + (gint64)timeout_ms * 1000;
    while (g_get_monotonic_time() < end) {
        while (g_main_context_pending(NULL))
            g_main_context_dispatch(NULL);
        if (cond(data))
            return true;
        g_usleep(5000);
    }
    return cond(data);
}

static bool cond_has_menu(void *d) { return mux_appmenu_has_menu((AppMenu *)d); }
static bool cond_no_menu(void *d) { return !mux_appmenu_has_menu((AppMenu *)d); }
static bool cond_activated(void *d)
{
    (void)d;
    return strstr(fake_output->str, "ACTIVATED new-window") != NULL;
}

int main(int argc, char **argv)
{
    assert(argc == 2); /* path to fake-menu-app.py */
    GError *err = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    assert(bus != NULL);

    am = mux_appmenu_new(bus, on_change, NULL);
    assert(mux_appmenu_start_registrar(am));

    /* spawn the fake app */
    fake_output = g_string_new("");
    char *fake_argv[] = { "python3", argv[1], "--headless", NULL };
    int out_fd = -1;
    assert(g_spawn_async_with_pipes(NULL, fake_argv, NULL,
                                    G_SPAWN_SEARCH_PATH |
                                        G_SPAWN_DO_NOT_REAP_CHILD,
                                    NULL, NULL, &fake_pid, NULL, &out_fd,
                                    NULL, &err));
    GIOChannel *ch = g_io_channel_unix_new(out_fd);
    /* NONBLOCK is essential: a blocking GIOChannel read inside the
     * watch callback would stall the whole main context */
    g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_encoding(ch, NULL, NULL);
    g_io_add_watch(ch, G_IO_IN, on_fake_out, NULL);

    /* focus it immediately — before it has even registered, to walk
     * the startup-race path in the registrar */
    mux_appmenu_set_focus(am, FAKE_APP_ID);
    assert(pump_until(cond_has_menu, am, 8000));

    /* titles + items parsed from org.gtk.Menus */
    assert(mux_appmenu_title_count(am) == 4);
    const char *want_titles[] = { "File", "Edit", "View", "Help" };
    for (int i = 0; i < 4; i++) {
        const AppMenuTitle *t = mux_appmenu_title(am, i);
        assert(t && strcmp(t->label, want_titles[i]) == 0);
    }
    const AppMenuTitle *file = mux_appmenu_title(am, 0);
    assert(file->items->len == 2);
    AppMenuItem *nw = g_ptr_array_index(file->items, 0);
    assert(strcmp(nw->label, "New Window") == 0);
    assert(strcmp(nw->action, "new-window") == 0);
    assert(nw->enabled);
    const AppMenuTitle *edit = mux_appmenu_title(am, 1);
    assert(edit->items->len == 2);
    AppMenuItem *trash = g_ptr_array_index(edit->items, 1);
    assert(strcmp(trash->label, "Move to Trash") == 0);
    assert(!trash->enabled); /* DescribeAll said disabled */
    puts("titles/items/enabled: OK");

    /* registrar query from a second connection */
    GDBusConnection *other = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    assert(other != NULL);
    GVariant *res = g_dbus_connection_call_sync(
        other, "com.canonical.AppMenu.Registrar",
        "/com/canonical/AppMenu/Registrar", "com.canonical.AppMenu.Registrar",
        "GetMenuForWindow", g_variant_new("(u)", FAKE_WINDOW_KEY),
        G_VARIANT_TYPE("(sooo)"), G_DBUS_CALL_FLAGS_NONE, 2000, NULL, &err);
    assert(res != NULL);
    const char *svc, *menubar, *winp, *appp;
    g_variant_get(res, "(&s&o&o&o)", &svc, &menubar, &winp, &appp);
    assert(svc[0] == ':');
    assert(strcmp(menubar, "/com/muxdesk/menu/fake") == 0);
    assert(strcmp(appp, "/com/muxdesk/actions/fake") == 0);
    g_variant_unref(res);
    puts("GetMenuForWindow: OK");

    /* activation reaches the app */
    mux_appmenu_activate(am, nw);
    assert(pump_until(cond_activated, NULL, 5000));
    puts("Activate -> app: OK");

    /* focus away clears, focus back restores */
    mux_appmenu_set_focus(am, "org.example.NoSuchApp");
    assert(pump_until(cond_no_menu, am, 3000));
    mux_appmenu_set_focus(am, FAKE_APP_ID);
    assert(pump_until(cond_has_menu, am, 5000));
    puts("focus away/back: OK");

    /* app exits -> menu disappears via NameOwnerChanged */
    kill(fake_pid, SIGTERM);
    assert(pump_until(cond_no_menu, am, 5000));
    puts("app exit clears menu: OK");

    g_spawn_close_pid(fake_pid);
    mux_appmenu_free(am);
    puts("test-appmenu: OK");
    return 0;
}
