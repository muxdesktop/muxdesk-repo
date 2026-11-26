#define _GNU_SOURCE
#include "appmenu.h"

#include <string.h>

#define REGISTRAR_NAME "com.canonical.AppMenu.Registrar"
#define REGISTRAR_PATH "/com/canonical/AppMenu/Registrar"
#define DBUS_NAME "org.freedesktop.DBus"
#define DBUS_PATH "/org/freedesktop/DBus"
#define CALL_TIMEOUT_MS 1000

/* Registrar signatures in the wild (all must work):
 *   spec:        RegisterWindow(u, s, o, o, o) — window, service,
 *                menubar, window actions path, app actions path
 *   our own apps RegisterWindow(u, s, o, o) — the MuxLike contract has
 *                ONE actions path, passed once (window app merged)
 *   canonical:   RegisterWindow(u, s, o) — window, service, menubar
 * GDBus object registration enforces exactly one declared signature,
 * so the registrar is implemented as a connection message filter
 * (see registrar_filter) that accepts all three shapes. */

typedef struct {
    guint32 window;
    char *sender; /* unique bus name of the registering app */
    char *service;
    char *menubar, *window_path, *app_path;
} Registration;

struct AppMenu {
    GDBusConnection *bus; /* borrowed */
    void (*on_change)(void *data);
    void *data;

    guint name_id, filter_id;
    GPtrArray *regs; /* Registration* */
    guint owner_watch_id;

    char *focus_app_id;
    char *focus_owner; /* resolved unique name for focus_app_id */

    /* current subscription (all NULL when no menu) */
    char *cur_service, *cur_menubar, *cur_window_path, *cur_app_path;
    guint subs[16];
    int n_subs;
    guint changed_id;
    GPtrArray *titles;      /* AppMenuTitle* */
    GHashTable *enabled_app; /* bare action name -> gboolean */
    GHashTable *enabled_win;
    bool refresh_queued;
};

/* ---------- small utils ---------- */

static void registration_free(Registration *r)
{
    g_free(r->sender);
    g_free(r->service);
    g_free(r->menubar);
    g_free(r->window_path);
    g_free(r->app_path);
    g_free(r);
}

static void item_free(AppMenuItem *it)
{
    g_free(it->label);
    g_free(it->action);
    if (it->target)
        g_variant_unref(it->target);
    g_free(it);
}

static void title_free(AppMenuTitle *t)
{
    g_free(t->label);
    if (t->items)
        g_ptr_array_free(t->items, TRUE);
    g_free(t);
}

static void notify_change(AppMenu *am)
{
    if (am->on_change)
        am->on_change(am->data);
}

/* ---------- menu model ---------- */

static void clear_titles(AppMenu *am)
{
    if (am->titles) {
        g_ptr_array_free(am->titles, TRUE);
        am->titles = NULL;
    }
}

/* Kept separate from clear_titles: refresh_current fetches the
 * enabled maps BEFORE build_titles runs (build_titles needs them),
 * and build_titles starts by clearing the old titles. */
static void clear_enabled(AppMenu *am)
{
    if (am->enabled_app) {
        g_hash_table_destroy(am->enabled_app);
        am->enabled_app = NULL;
    }
    if (am->enabled_win) {
        g_hash_table_destroy(am->enabled_win);
        am->enabled_win = NULL;
    }
}

/* org.gtk.Actions.DescribeAll at one path -> name->enabled hash.
 * Parsed with explicit child values: the (bgav) tuple's "av" member
 * makes format-string iteration error-prone, and we only need the
 * boolean anyway. */
static GHashTable *describe_all(AppMenu *am, const char *path)
{
    if (!path)
        return NULL;
    GError *err = NULL;
    GVariant *res = g_dbus_connection_call_sync(
        am->bus, am->cur_service, path, "org.gtk.Actions", "DescribeAll",
        NULL, G_VARIANT_TYPE("(a{s(bgav)})"), G_DBUS_CALL_FLAGS_NONE,
        CALL_TIMEOUT_MS, NULL, &err);
    if (!res) {
        g_clear_error(&err);
        return NULL;
    }
    GHashTable *map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            NULL);
    GVariant *dict = g_variant_get_child_value(res, 0);
    GVariantIter iter;
    GVariant *entry;
    g_variant_iter_init(&iter, dict);
    while ((entry = g_variant_iter_next_value(&iter)) != NULL) {
        GVariant *k = g_variant_get_child_value(entry, 0);
        GVariant *v = g_variant_get_child_value(entry, 1);
        GVariant *en = g_variant_get_child_value(v, 0);
        /* 1 = enabled, 2 = disabled — 0/NULL must stay "not in map",
         * or a disabled action would look the same as an unknown one */
        g_hash_table_insert(map, g_strdup(g_variant_get_string(k, NULL)),
                            GINT_TO_POINTER(g_variant_get_boolean(en) ? 1
                                                                      : 2));
        g_variant_unref(en);
        g_variant_unref(v);
        g_variant_unref(k);
        g_variant_unref(entry);
    }
    g_variant_unref(dict);
    g_variant_unref(res);
    return map;
}

static bool action_enabled(AppMenu *am, const char *action)
{
    if (!action)
        return true;
    const char *dot = strchr(action, '.');
    const char *bare = dot ? dot + 1 : action;
    GHashTable *map = NULL;
    if (dot && strncmp(action, "win", 3) == 0 && dot == action + 3)
        map = am->enabled_win;
    else
        map = am->enabled_app;
    if (!map)
        return true;
    gpointer v = g_hash_table_lookup(map, bare);
    return v == NULL || GPOINTER_TO_INT(v) == 1;
}

/* Collect every (group, section) from a list of Start() replies into
 * one sections hash: key (group<<32 | section) -> GPtrArray of item
 * dict variants. org.gtk.Menus splits a menubar into groups: group 0
 * is the root, every submenu is its own group — so several Start()
 * replies have to be merged before links can be resolved. */
static GHashTable *collect_sections(GPtrArray *replies)
{
    GHashTable *sections = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                                 NULL, NULL);
    for (guint r = 0; r < replies->len; r++) {
        GVariant *groups = g_variant_get_child_value(
            g_ptr_array_index(replies, r), 0);
        GVariantIter giter;
        guint32 sub, sec;
        GVariant *items_var;
        g_variant_iter_init(&giter, groups);
        while (g_variant_iter_next(&giter, "(uu@aa{sv})", &sub, &sec,
                                   &items_var)) {
            GPtrArray *list = g_ptr_array_new_with_free_func(
                (GDestroyNotify)g_variant_unref);
            GVariantIter iiter;
            GVariant *item;
            g_variant_iter_init(&iiter, items_var);
            while ((item = g_variant_iter_next_value(&iiter)) != NULL)
                g_ptr_array_add(list, item);
            guint64 key = ((guint64)sub << 32) | sec;
            GPtrArray *old = g_hash_table_lookup(sections,
                                                 GUINT_TO_POINTER(key));
            if (old)
                g_ptr_array_free(old, TRUE);
            g_hash_table_insert(sections, GUINT_TO_POINTER(key), list);
            g_variant_unref(items_var);
        }
        g_variant_unref(groups);
    }
    return sections;
}

static void free_sections(GHashTable *sections)
{
    GHashTableIter hiter;
    gpointer k, v;
    g_hash_table_iter_init(&hiter, sections);
    while (g_hash_table_iter_next(&hiter, &k, &v))
        g_ptr_array_free(v, TRUE);
    g_hash_table_destroy(sections);
}

/* Build titles from the merged sections; the root is group 0,
 * section 0. */
static void build_titles(AppMenu *am, GPtrArray *replies)
{
    clear_titles(am);
    am->titles = g_ptr_array_new_with_free_func((GDestroyNotify)title_free);

    GHashTable *sections = collect_sections(replies);
    GPtrArray *top = g_hash_table_lookup(sections, GUINT_TO_POINTER(0));
    if (!top) {
        free_sections(sections);
        return;
    }

    for (guint i = 0; i < top->len; i++) {
        GVariant *item = g_ptr_array_index(top, i);
        AppMenuTitle *title = g_new0(AppMenuTitle, 1);
        title->items = g_ptr_array_new_with_free_func(
            (GDestroyNotify)item_free);
        const char *label = NULL;
        g_variant_lookup(item, "label", "&s", &label);
        title->label = g_strdup(label ? label : "");

        GVariant *link = g_variant_lookup_value(item, ":submenu", NULL);
        if (!link)
            link = g_variant_lookup_value(item, ":section", NULL);
        if (link) {
            guint32 lsub = 0, lsec = 0;
            g_variant_get(link, "(uu)", &lsub, &lsec);
            g_variant_unref(link);
            GPtrArray *children = g_hash_table_lookup(
                sections, GUINT_TO_POINTER(((guint64)lsub << 32) | lsec));
            if (children) {
                for (guint j = 0; j < children->len; j++) {
                    GVariant *ci = g_ptr_array_index(children, j);
                    AppMenuItem *ai = g_new0(AppMenuItem, 1);
                    const char *cl = NULL, *ca = NULL;
                    g_variant_lookup(ci, "label", "&s", &cl);
                    g_variant_lookup(ci, "action", "&s", &ca);
                    ai->label = g_strdup(cl ? cl : "");
                    ai->action = g_strdup(ca);
                    ai->target = g_variant_lookup_value(ci, "target", NULL);
                    GVariant *sub1 = g_variant_lookup_value(ci, ":submenu",
                                                            NULL);
                    GVariant *sub2 = g_variant_lookup_value(ci, ":section",
                                                            NULL);
                    ai->has_children = sub1 != NULL || sub2 != NULL;
                    if (sub1)
                        g_variant_unref(sub1);
                    if (sub2)
                        g_variant_unref(sub2);
                    ai->enabled = action_enabled(am, ai->action);
                    g_ptr_array_add(title->items, ai);
                }
            }
        } else {
            /* top-level plain action: it is its own single dropdown item */
            const char *act = NULL;
            g_variant_lookup(item, "action", "&s", &act);
            if (act) {
                AppMenuItem *ai = g_new0(AppMenuItem, 1);
                ai->label = g_strdup(title->label);
                ai->action = g_strdup(act);
                ai->target = g_variant_lookup_value(item, "target", NULL);
                ai->enabled = action_enabled(am, ai->action);
                g_ptr_array_add(title->items, ai);
            }
        }
        g_ptr_array_add(am->titles, title);
    }

    free_sections(sections);
}

/* One Start() call for the given group ids; NULL on error. */
static GVariant *menus_start(AppMenu *am, GArray *ids)
{
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("au"));
    for (guint i = 0; i < ids->len; i++)
        g_variant_builder_add(&b, "u", g_array_index(ids, guint32, i));
    GError *err = NULL;
    GVariant *res = g_dbus_connection_call_sync(
        am->bus, am->cur_service, am->cur_menubar, "org.gtk.Menus", "Start",
        g_variant_new("(au)", &b), G_VARIANT_TYPE("(a(uuaa{sv}))"),
        G_DBUS_CALL_FLAGS_NONE, CALL_TIMEOUT_MS, NULL, &err);
    if (!res) {
        g_printerr("mux-panel: appmenu Start failed for %s: %s\n",
                   am->cur_service, err ? err->message : "?");
        g_clear_error(&err);
    }
    return res;
}

/* Scan a Start() reply for :submenu/:section links whose group id is
 * not in `have` yet; append those ids to `want`. */
static void scan_links(GVariant *reply, GHashTable *have, GArray *want)
{
    GVariant *groups = g_variant_get_child_value(reply, 0);
    GVariantIter giter;
    guint32 sub, sec;
    GVariant *items_var;
    g_variant_iter_init(&giter, groups);
    while (g_variant_iter_next(&giter, "(uu@aa{sv})", &sub, &sec,
                               &items_var)) {
        GVariantIter iiter;
        GVariant *item;
        g_variant_iter_init(&iiter, items_var);
        while ((item = g_variant_iter_next_value(&iiter)) != NULL) {
            const char *keys[] = { ":submenu", ":section", NULL };
            for (int k = 0; keys[k]; k++) {
                GVariant *link = g_variant_lookup_value(item, keys[k], NULL);
                if (!link)
                    continue;
                guint32 gid = 0, sid = 0;
                g_variant_get(link, "(uu)", &gid, &sid);
                g_variant_unref(link);
                if (!g_hash_table_contains(have, GUINT_TO_POINTER(gid))) {
                    g_hash_table_insert(have, GUINT_TO_POINTER(gid),
                                        GUINT_TO_POINTER(1));
                    g_array_append_val(want, gid);
                }
            }
            g_variant_unref(item);
        }
        g_variant_unref(items_var);
    }
    g_variant_unref(groups);
}

/* (Re)subscribe to the current service's menubar and rebuild.
 * org.gtk.Menus splits the model into groups (root = 0, each submenu
 * its own group), so: Start([0]), follow the links, Start those
 * groups too, repeat until no new group shows up (bounded rounds). */
static void refresh_current(AppMenu *am)
{
    if (!am->bus || !am->cur_service)
        return;
    GPtrArray *replies = g_ptr_array_new_with_free_func(
        (GDestroyNotify)g_variant_unref);
    GHashTable *have = g_hash_table_new(g_direct_hash, g_direct_equal);
    GArray *want = g_array_new(FALSE, FALSE, sizeof(guint32));
    guint32 zero = 0;
    g_array_append_val(want, zero);
    g_hash_table_insert(have, GUINT_TO_POINTER(0), GUINT_TO_POINTER(1));

    am->n_subs = 0;
    for (int round = 0; round < 4 && want->len > 0; round++) {
        GVariant *res = menus_start(am, want);
        if (!res)
            break;
        g_ptr_array_add(replies, res);
        for (guint i = 0; i < want->len && am->n_subs < 16; i++)
            am->subs[am->n_subs++] = g_array_index(want, guint32, i);
        g_array_remove_range(want, 0, want->len);
        scan_links(res, have, want);
    }
    g_array_free(want, TRUE);
    g_hash_table_destroy(have);

    if (replies->len == 0) {
        clear_titles(am);
        clear_enabled(am);
        g_ptr_array_free(replies, TRUE);
        notify_change(am);
        return;
    }
    clear_enabled(am);
    am->enabled_app = describe_all(am, am->cur_app_path);
    am->enabled_win = describe_all(am, am->cur_window_path);
    build_titles(am, replies);
    g_ptr_array_free(replies, TRUE);
    notify_change(am);
}

static gboolean refresh_idle_cb(void *data)
{
    AppMenu *am = data;
    am->refresh_queued = false;
    refresh_current(am);
    return G_SOURCE_REMOVE;
}

static void queue_refresh(AppMenu *am)
{
    if (!am->refresh_queued) {
        am->refresh_queued = true;
        g_idle_add(refresh_idle_cb, am);
    }
}

static void menus_changed_cb(GDBusConnection *bus, const char *sender,
                             const char *path, const char *iface,
                             const char *signal_name, GVariant *params,
                             void *data)
{
    (void)bus; (void)sender; (void)path; (void)iface; (void)signal_name;
    (void)params;
    queue_refresh((AppMenu *)data);
}

static void unsubscribe_current(AppMenu *am)
{
    if (am->changed_id) {
        g_dbus_connection_signal_unsubscribe(am->bus, am->changed_id);
        am->changed_id = 0;
    }
    if (am->bus && am->cur_service && am->n_subs > 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("au"));
        for (int i = 0; i < am->n_subs; i++)
            g_variant_builder_add(&b, "u", am->subs[i]);
        g_dbus_connection_call(am->bus, am->cur_service, am->cur_menubar,
                               "org.gtk.Menus", "End",
                               g_variant_new("(au)", &b),
                               NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL,
                               NULL);
    }
    am->n_subs = 0;
    g_clear_pointer(&am->cur_service, g_free);
    g_clear_pointer(&am->cur_menubar, g_free);
    g_clear_pointer(&am->cur_window_path, g_free);
    g_clear_pointer(&am->cur_app_path, g_free);
    clear_titles(am);
    clear_enabled(am);
}

static void subscribe_to(AppMenu *am, const Registration *reg)
{
    unsubscribe_current(am);
    am->cur_service = g_strdup(reg->service);
    am->cur_menubar = g_strdup(reg->menubar);
    am->cur_window_path = g_strdup(reg->window_path);
    am->cur_app_path = g_strdup(reg->app_path);
    am->changed_id = g_dbus_connection_signal_subscribe(
        am->bus, am->cur_service, "org.gtk.Menus", "Changed", am->cur_menubar,
        NULL, G_DBUS_SIGNAL_FLAGS_NONE, menus_changed_cb, am, NULL);
    refresh_current(am);
}

/* ---------- focus resolution ---------- */

static Registration *find_by_sender(AppMenu *am, const char *sender)
{
    if (!sender)
        return NULL;
    for (guint i = 0; i < am->regs->len; i++) {
        Registration *r = g_ptr_array_index(am->regs, i);
        if (g_strcmp0(r->sender, sender) == 0 ||
            g_strcmp0(r->service, sender) == 0)
            return r;
    }
    return NULL;
}

static void resolve_focus(AppMenu *am)
{
    if (!am->bus || !am->focus_app_id) {
        unsubscribe_current(am);
        notify_change(am);
        return;
    }
    GError *err = NULL;
    GVariant *res = g_dbus_connection_call_sync(
        am->bus, DBUS_NAME, DBUS_PATH, DBUS_NAME, "GetNameOwner",
        g_variant_new("(s)", am->focus_app_id), G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE, 500, NULL, &err);
    const char *owner = NULL;
    char *owner_copy = NULL;
    if (res) {
        g_variant_get(res, "(&s)", &owner);
        owner_copy = g_strdup(owner);
        g_variant_unref(res);
    } else {
        g_clear_error(&err);
    }
    g_free(am->focus_owner);
    am->focus_owner = owner_copy;

    Registration *reg = find_by_sender(am, am->focus_owner);
    if (!reg && am->focus_owner == NULL)
        reg = find_by_sender(am, am->focus_app_id); /* service == app_id */
    if (!reg) {
        if (am->cur_service)
            unsubscribe_current(am);
        notify_change(am);
        return;
    }
    if (am->cur_service && g_strcmp0(am->cur_service, reg->service) == 0 &&
        g_strcmp0(am->cur_menubar, reg->menubar) == 0) {
        refresh_current(am); /* same app, model may be new */
        return;
    }
    subscribe_to(am, reg);
}

static gboolean resolve_idle_cb(void *data)
{
    AppMenu *am = data;
    am->refresh_queued = false;
    resolve_focus(am);
    return G_SOURCE_REMOVE;
}

/* ---------- registrar (message filter — see signature note above) --- */

static void handle_register(AppMenu *am, const char *sender, guint32 window,
                            const char *service, const char *menubar,
                            const char *win_path, const char *app_path)
{
    /* replace an existing registration for the same window+sender */
    for (gint i = (gint)am->regs->len - 1; i >= 0; i--) {
        Registration *r = g_ptr_array_index(am->regs, i);
        if (r->window == window && g_strcmp0(r->sender, sender) == 0)
            g_ptr_array_remove_index(am->regs, i);
    }
    Registration *r = g_new0(Registration, 1);
    r->window = window;
    r->sender = g_strdup(sender);
    r->service = g_strdup(service);
    r->menubar = g_strdup(menubar);
    r->window_path = g_strdup(win_path);
    r->app_path = g_strdup(app_path);
    g_ptr_array_add(am->regs, r);
    /* The newly registered app may be the focused one already — either
     * its sender matches the resolved focus owner, or focus resolution
     * ran before the app owned its name / registered (startup race)
     * and we still have no menu for the focused app. */
    if (!am->refresh_queued &&
        ((am->focus_owner && g_strcmp0(sender, am->focus_owner) == 0) ||
         (am->focus_app_id && !am->cur_service))) {
        am->refresh_queued = true;
        g_idle_add(resolve_idle_cb, am);
    }
}

static void send_reply(GDBusConnection *bus, GDBusMessage *reply)
{
    GError *err = NULL;
    if (!g_dbus_connection_send_message(bus, reply,
                                        G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL,
                                        &err)) {
        g_printerr("mux-panel: registrar reply failed: %s\n", err->message);
        g_clear_error(&err);
    }
    g_object_unref(reply);
}

static GDBusMessage *registrar_filter(GDBusConnection *bus, GDBusMessage *msg,
                                      gboolean incoming, void *data)
{
    AppMenu *am = data;
    if (!incoming ||
        g_dbus_message_get_message_type(msg) != G_DBUS_MESSAGE_TYPE_METHOD_CALL ||
        g_strcmp0(g_dbus_message_get_interface(msg), REGISTRAR_NAME) != 0 ||
        g_strcmp0(g_dbus_message_get_path(msg), REGISTRAR_PATH) != 0)
        return msg; /* not ours — pass through */

    const char *member = g_dbus_message_get_member(msg);
    const char *sender = g_dbus_message_get_sender(msg);
    GVariant *params = g_dbus_message_get_body(msg);
    const char *sig = params ? g_variant_get_type_string(params) : "";

    if (g_strcmp0(member, "RegisterWindow") == 0) {
        guint32 window = 0;
        const char *service = NULL, *menubar = NULL, *win = NULL, *app = NULL;
        if (g_strcmp0(sig, "(usooo)") == 0) {
            g_variant_get(params, "(u&s&o&o&o)", &window, &service, &menubar,
                          &win, &app);
        } else if (g_strcmp0(sig, "(usoo)") == 0) {
            g_variant_get(params, "(u&s&o&o)", &window, &service, &menubar,
                          &app);
            win = app; /* single contract actions path serves both */
        } else if (g_strcmp0(sig, "(uso)") == 0) {
            g_variant_get(params, "(u&s&o)", &window, &service, &menubar);
        } else {
            send_reply(bus, g_dbus_message_new_method_error_literal(
                                msg, "org.freedesktop.DBus.Error.InvalidArgs",
                                "RegisterWindow: unsupported signature"));
            return NULL;
        }
        handle_register(am, sender, window, service, menubar, win, app);
        GDBusMessage *reply = g_dbus_message_new_method_reply(msg);
        g_dbus_message_set_body(reply, g_variant_new("()"));
        send_reply(bus, reply);
        return NULL;
    }
    if (g_strcmp0(member, "UnregisterWindow") == 0) {
        guint32 window = 0;
        if (params)
            g_variant_get(params, "(u)", &window);
        for (gint i = (gint)am->regs->len - 1; i >= 0; i--) {
            Registration *r = g_ptr_array_index(am->regs, i);
            if (r->window == window && g_strcmp0(r->sender, sender) == 0)
                g_ptr_array_remove_index(am->regs, i);
        }
        GDBusMessage *reply = g_dbus_message_new_method_reply(msg);
        g_dbus_message_set_body(reply, g_variant_new("()"));
        send_reply(bus, reply);
        return NULL;
    }
    if (g_strcmp0(member, "GetMenuForWindow") == 0) {
        guint32 window = 0;
        if (params)
            g_variant_get(params, "(u)", &window);
        Registration *found = NULL;
        for (guint i = 0; i < am->regs->len; i++) {
            Registration *r = g_ptr_array_index(am->regs, i);
            if (r->window == window) {
                found = r;
                break;
            }
        }
        if (found) {
            GDBusMessage *reply = g_dbus_message_new_method_reply(msg);
            g_dbus_message_set_body(reply, g_variant_new("(sooo)",
                                                         found->service,
                                                         found->menubar,
                                                         found->window_path,
                                                         found->app_path));
            send_reply(bus, reply);
        } else {
            send_reply(bus, g_dbus_message_new_method_error_literal(
                                msg,
                                "com.canonical.AppMenu.Registrar.NoSuchWindow",
                                "No menu registered for this window"));
        }
        return NULL;
    }
    send_reply(bus, g_dbus_message_new_method_error_literal(
                        msg, "org.freedesktop.DBus.Error.UnknownMethod",
                        member ? member : ""));
    return NULL;
}

static void name_acquired_cb(GDBusConnection *bus, const char *name,
                             void *data)
{
    (void)bus; (void)name; (void)data;
    g_printerr("mux-panel: global menu registrar active (%s)\n",
               REGISTRAR_NAME);
}

static void name_lost_cb(GDBusConnection *bus, const char *name, void *data)
{
    (void)bus; (void)name;
    AppMenu *am = data;
    g_printerr("mux-panel: registrar name lost — another registrar owns "
               "%s; global menu disabled\n", REGISTRAR_NAME);
    if (am->cur_service)
        unsubscribe_current(am);
    notify_change(am);
}

static void name_owner_changed_cb(GDBusConnection *bus, const char *sender,
                                  const char *path, const char *iface,
                                  const char *signal_name, GVariant *params,
                                  void *data)
{
    (void)bus; (void)sender; (void)path; (void)iface; (void)signal_name;
    AppMenu *am = data;
    const char *name, *old_owner, *new_owner;
    g_variant_get(params, "(&s&s&s)", &name, &old_owner, &new_owner);
    if (new_owner && *new_owner)
        return; /* only disappearances matter */
    bool dropped_current = false;
    for (gint i = (gint)am->regs->len - 1; i >= 0; i--) {
        Registration *r = g_ptr_array_index(am->regs, i);
        if (g_strcmp0(r->sender, name) == 0) {
            if (am->cur_service && g_strcmp0(am->cur_service, r->service) == 0)
                dropped_current = true;
            g_ptr_array_remove_index(am->regs, i);
        }
    }
    if (dropped_current) {
        unsubscribe_current(am);
        notify_change(am);
    }
}

/* ---------- public API ---------- */

AppMenu *mux_appmenu_new(GDBusConnection *bus, void (*on_change)(void *),
                         void *data)
{
    AppMenu *am = g_new0(AppMenu, 1);
    am->bus = bus;
    am->on_change = on_change;
    am->data = data;
    am->regs = g_ptr_array_new_with_free_func((GDestroyNotify)registration_free);
    return am;
}

bool mux_appmenu_start_registrar(AppMenu *am)
{
    if (!am->bus)
        return false;
    am->filter_id = g_dbus_connection_add_filter(am->bus, registrar_filter,
                                                 am, NULL);
    am->name_id = g_bus_own_name_on_connection(
        am->bus, REGISTRAR_NAME, G_BUS_NAME_OWNER_FLAGS_NONE,
        name_acquired_cb, name_lost_cb, am, NULL);
    am->owner_watch_id = g_dbus_connection_signal_subscribe(
        am->bus, DBUS_NAME, DBUS_NAME, "NameOwnerChanged", DBUS_PATH, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, name_owner_changed_cb, am, NULL);
    return true;
}

void mux_appmenu_set_focus(AppMenu *am, const char *app_id)
{
    if (g_strcmp0(am->focus_app_id, app_id) == 0)
        return;
    g_free(am->focus_app_id);
    am->focus_app_id = g_strdup(app_id);
    resolve_focus(am);
}

bool mux_appmenu_has_menu(AppMenu *am)
{
    return am->titles && am->titles->len > 0;
}

int mux_appmenu_title_count(AppMenu *am)
{
    return am->titles ? (int)am->titles->len : 0;
}

const AppMenuTitle *mux_appmenu_title(AppMenu *am, int index)
{
    if (!am->titles || index < 0 || index >= (int)am->titles->len)
        return NULL;
    return g_ptr_array_index(am->titles, index);
}

void mux_appmenu_activate(AppMenu *am, const AppMenuItem *item)
{
    if (!am->bus || !am->cur_service || !item || !item->action)
        return;
    const char *dot = strchr(item->action, '.');
    const char *bare = dot ? dot + 1 : item->action;
    const char *path = am->cur_app_path;
    if (dot && strncmp(item->action, "win", 3) == 0 && dot == item->action + 3)
        path = am->cur_window_path;
    if (!path)
        path = am->cur_app_path;
    if (!path)
        return;
    GVariantBuilder params_b, platform_b;
    g_variant_builder_init(&params_b, G_VARIANT_TYPE("av"));
    if (item->target)
        g_variant_builder_add(&params_b, "v", item->target);
    g_variant_builder_init(&platform_b, G_VARIANT_TYPE("a{sv}"));
    GVariant *args = g_variant_new("(sava{sv})", bare, &params_b, &platform_b);
    g_dbus_connection_call(am->bus, am->cur_service, path, "org.gtk.Actions",
                           "Activate", args, NULL, G_DBUS_CALL_FLAGS_NONE,
                           CALL_TIMEOUT_MS, NULL, NULL, NULL);
}

void mux_appmenu_free(AppMenu *am)
{
    if (!am)
        return;
    unsubscribe_current(am);
    if (am->bus && am->owner_watch_id)
        g_dbus_connection_signal_unsubscribe(am->bus, am->owner_watch_id);
    if (am->bus && am->filter_id)
        g_dbus_connection_remove_filter(am->bus, am->filter_id);
    if (am->name_id)
        g_bus_unown_name(am->name_id);
    g_ptr_array_free(am->regs, TRUE);
    g_free(am->focus_app_id);
    g_free(am->focus_owner);
    g_free(am);
}
