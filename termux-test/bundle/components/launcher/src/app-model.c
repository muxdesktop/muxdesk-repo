/* app-model.c — দেখো app-model.h */
#include "app-model.h"
#include "search-engine.h"

#include <string.h>

/* ================= MuxAppItem ================= */

struct _MuxAppItem {
    GObject parent_instance;
    GDesktopAppInfo *info; /* owned */
};

G_DEFINE_TYPE (MuxAppItem, mux_app_item, G_TYPE_OBJECT)

static void
mux_app_item_finalize (GObject *obj)
{
    MuxAppItem *self = MUX_APP_ITEM (obj);
    g_clear_object (&self->info);
    G_OBJECT_CLASS (mux_app_item_parent_class)->finalize (obj);
}

static void
mux_app_item_class_init (MuxAppItemClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = mux_app_item_finalize;
}

static void
mux_app_item_init (MuxAppItem *self) { (void) self; }

MuxAppItem *
mux_app_item_new (GDesktopAppInfo *info)
{
    MuxAppItem *self = g_object_new (MUX_TYPE_APP_ITEM, NULL);
    self->info = g_object_ref (info);
    return self;
}

GDesktopAppInfo *mux_app_item_get_info (MuxAppItem *self) { return self->info; }

const char *
mux_app_item_get_name (MuxAppItem *self)
{
    const char *n = g_app_info_get_display_name (G_APP_INFO (self->info));
    return n != NULL ? n : g_app_info_get_id (G_APP_INFO (self->info));
}

const char *
mux_app_item_get_id (MuxAppItem *self)
{
    return g_app_info_get_id (G_APP_INFO (self->info));
}

GIcon *
mux_app_item_get_icon (MuxAppItem *self)
{
    return g_app_info_get_icon (G_APP_INFO (self->info));
}

const char *const *
mux_app_item_get_keywords (MuxAppItem *self)
{
    return g_desktop_app_info_get_keywords (self->info);
}

gboolean
mux_app_item_launch (MuxAppItem *self, GError **err)
{
    return g_app_info_launch (G_APP_INFO (self->info), NULL, NULL, err);
}

/* ================= MuxAppModel ================= */

struct _MuxAppModel {
    GObject parent_instance;
    GPtrArray *items;          /* MuxAppItem, owned */
    GPtrArray *monitors;       /* GFileMonitor, owned */
};

G_DEFINE_TYPE (MuxAppModel, mux_app_model, G_TYPE_OBJECT)

enum { SIGNAL_CHANGED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void
mux_app_model_finalize (GObject *obj)
{
    MuxAppModel *self = MUX_APP_MODEL (obj);
    g_ptr_array_unref (self->items);
    g_ptr_array_unref (self->monitors);
    G_OBJECT_CLASS (mux_app_model_parent_class)->finalize (obj);
}

static void
mux_app_model_class_init (MuxAppModelClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = mux_app_model_finalize;
    signals[SIGNAL_CHANGED] = g_signal_new (
        "changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

/* GPtrArray/GArray sort comparator — GLib qsort-ধাঁচে element-এর
 * pointer পাঠায়, তাই deref করতে হয়। (যাচাই: tests + sortcheck —
 * সরাসরি cast করলে segfault হয়, এটাই ঠিক রূপ।) */
static gint
compare_by_name (gconstpointer a, gconstpointer b)
{
    MuxAppItem *ia = *(MuxAppItem *const *) a;
    MuxAppItem *ib = *(MuxAppItem *const *) b;
    g_autofree char *na = g_utf8_casefold (mux_app_item_get_name (ia), -1);
    g_autofree char *nb = g_utf8_casefold (mux_app_item_get_name (ib), -1);
    return g_strcmp0 (na, nb);
}

static void
load_items (MuxAppModel *self)
{
    g_ptr_array_set_size (self->items, 0);
    GList *all = g_app_info_get_all ();
    for (GList *l = all; l != NULL; l = l->next) {
        GAppInfo *info = l->data;
        /* launcher-এ দেখানোর মতো app-ই নাও: NoDisplay/Hidden বাদ */
        if (!G_IS_DESKTOP_APP_INFO (info))
            continue;
        if (!g_app_info_should_show (info))
            continue;
        g_ptr_array_add (self->items,
                         mux_app_item_new (G_DESKTOP_APP_INFO (info)));
    }
    g_list_free_full (all, g_object_unref);
    g_ptr_array_sort (self->items, compare_by_name);
}

/* monitor callback reload-কে idle-তে ডাকে (debounce-এর হালকা রূপ:
 * pacman একসাথে অনেক ফাইল লিখলেও idle একবারই reload করবে পরপর
 * event গুলো জমে গেলে — পুরো debounce timer পরে যোগ করা যাবে) */
static gboolean
reload_idle (gpointer user_data)
{
    MuxAppModel *self = user_data;
    mux_app_model_reload (self);
    g_object_unref (self);
    return G_SOURCE_REMOVE;
}

static void
on_dir_changed (GFileMonitor *mon, GFile *file, GFile *other,
                     GFileMonitorEvent event, gpointer user_data)
{
    (void) mon; (void) file; (void) other;
    if (event == G_FILE_MONITOR_EVENT_CREATED ||
        event == G_FILE_MONITOR_EVENT_DELETED) {
        MuxAppModel *self = user_data;
        g_object_ref (self);
        g_idle_add (reload_idle, self);
    }
}

static void
watch_app_dirs (MuxAppModel *self)
{
    /* standard applications folder গুলো — XDG data dirs অনুযায়ী */
    g_autoptr (GPtrArray) dirs = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (dirs, g_build_filename (g_get_user_data_dir (),
                                             "applications", NULL));
    const char *const *sys = g_get_system_data_dirs ();
    for (int i = 0; sys[i] != NULL; i++)
        g_ptr_array_add (dirs, g_build_filename (sys[i], "applications", NULL));

    for (guint i = 0; i < dirs->len; i++) {
        const char *path = g_ptr_array_index (dirs, i);
        g_autoptr (GFile) f = g_file_new_for_path (path);
        g_autoptr (GError) err = NULL;
        GFileMonitor *mon = g_file_monitor_directory (
            f, G_FILE_MONITOR_NONE, NULL, &err);
        if (mon != NULL) {
            g_signal_connect (mon, "changed",
                              G_CALLBACK (on_dir_changed), self);
            g_ptr_array_add (self->monitors, mon);
        }
    }
}

static void
mux_app_model_init (MuxAppModel *self)
{
    self->items = g_ptr_array_new_with_free_func (g_object_unref);
    self->monitors = g_ptr_array_new_with_free_func (g_object_unref);
    load_items (self);
    watch_app_dirs (self);
}

MuxAppModel *
mux_app_model_new (void)
{
    return g_object_new (MUX_TYPE_APP_MODEL, NULL);
}

GPtrArray *
mux_app_model_get_all (MuxAppModel *self)
{
    return self->items;
}

void
mux_app_model_reload (MuxAppModel *self)
{
    load_items (self);
    g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

typedef struct { MuxAppItem *item; int score; } Scored;

static gint
compare_scored (gconstpointer a, gconstpointer b)
{
    const Scored *sa = a, *sb = b;
    if (sa->score != sb->score)
        return sb->score - sa->score; /* বেশি score আগে */
    return compare_by_name (&sa->item, &sb->item);
}

GPtrArray *
mux_app_model_search (MuxAppModel *self, const char *query, int limit)
{
    GPtrArray *out = g_ptr_array_new (); /* item model-এর, ref নেওয়া হয়নি */

    if (mux_search_query_empty (query)) {
        for (guint i = 0; i < self->items->len; i++) {
            if (limit > 0 && (int) out->len >= limit)
                break;
            g_ptr_array_add (out, g_ptr_array_index (self->items, i));
        }
        return out;
    }

    g_autoptr (GArray) scored = g_array_new (FALSE, FALSE, sizeof (Scored));
    for (guint i = 0; i < self->items->len; i++) {
        MuxAppItem *item = g_ptr_array_index (self->items, i);
        int s = mux_search_score (query, mux_app_item_get_name (item),
                                  mux_app_item_get_keywords (item));
        if (s >= 0) {
            Scored sc = { item, s };
            g_array_append_val (scored, sc);
        }
    }
    g_array_sort (scored, compare_scored);
    for (guint i = 0; i < scored->len; i++) {
        if (limit > 0 && (int) out->len >= limit)
            break;
        g_ptr_array_add (out, g_array_index (scored, Scored, i).item);
    }
    return out;
}
