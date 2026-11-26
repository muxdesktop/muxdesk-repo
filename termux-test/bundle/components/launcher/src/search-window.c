/* search-window.c — দেখো search-window.h */
#include "search-window.h"
#include "layer-util.h"
#include "animation.h"
#include "search-engine.h"

struct _MuxSearchWindow {
    GtkWindow parent_instance;
    MuxAppModel *model;          /* ref ধরা */
    MuxLauncherConfig *cfg;      /* borrowed */
    GtkSearchEntry *entry;
    GtkListBox *list;
    GtkLabel *hint;
    GCancellable *file_cancel;   /* চলতি file search */
    guint debounce_id;
    guint query_stamp;           /* পুরনো async ফলাফল ফেলতে */
};

G_DEFINE_TYPE (MuxSearchWindow, mux_search_window, GTK_TYPE_WINDOW)

/* row-তে কী আছে: "app" (MuxAppItem*) বা "file" (path char*) */
static GtkListBoxRow *
make_app_row (MuxAppItem *item)
{
    GtkWidget *row = gtk_list_box_row_new ();
    g_object_set_data_full (G_OBJECT (row), "mux-kind",
                            g_strdup ("app"), g_free);
    g_object_set_data (G_OBJECT (row), "mux-app-item", item);

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (box, 6);
    gtk_widget_set_margin_bottom (box, 6);
    gtk_widget_set_margin_start (box, 12);
    gtk_widget_set_margin_end (box, 12);

    GIcon *icon = mux_app_item_get_icon (item);
    GtkWidget *img = icon != NULL
        ? gtk_image_new_from_gicon (icon)
        : gtk_image_new_from_icon_name ("application-x-executable");
    gtk_image_set_pixel_size (GTK_IMAGE (img), 32);
    gtk_box_append (GTK_BOX (box), img);

    GtkWidget *label = gtk_label_new (mux_app_item_get_name (item));
    gtk_widget_add_css_class (label, "result-name");
    gtk_box_append (GTK_BOX (box), label);

    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
    return GTK_LIST_BOX_ROW (row);
}

static GtkListBoxRow *
make_file_row (MuxFileResult *fr)
{
    GtkWidget *row = gtk_list_box_row_new ();
    g_object_set_data_full (G_OBJECT (row), "mux-kind",
                            g_strdup ("file"), g_free);
    g_object_set_data_full (G_OBJECT (row), "mux-file-path",
                            g_strdup (mux_file_result_get_path (fr)), g_free);

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top (box, 6);
    gtk_widget_set_margin_bottom (box, 6);
    gtk_widget_set_margin_start (box, 12);
    gtk_widget_set_margin_end (box, 12);

    GIcon *icon = mux_file_result_get_icon (fr);
    GtkWidget *img = icon != NULL
        ? gtk_image_new_from_gicon (icon)
        : gtk_image_new_from_icon_name ("text-x-generic");
    gtk_image_set_pixel_size (GTK_IMAGE (img), 32);
    gtk_box_append (GTK_BOX (box), img);

    GtkWidget *texts = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *name = gtk_label_new (mux_file_result_get_display_name (fr));
    gtk_widget_add_css_class (name, "result-name");
    gtk_label_set_xalign (GTK_LABEL (name), 0.0);
    gtk_box_append (GTK_BOX (texts), name);
    GtkWidget *path = gtk_label_new (mux_file_result_get_path (fr));
    gtk_widget_add_css_class (path, "result-path");
    gtk_label_set_xalign (GTK_LABEL (path), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (path), PANGO_ELLIPSIZE_START);
    gtk_box_append (GTK_BOX (texts), path);
    gtk_box_append (GTK_BOX (box), texts);

    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
    return GTK_LIST_BOX_ROW (row);
}

static GtkListBoxRow *
make_section_row (const char *text)
{
    GtkWidget *row = gtk_list_box_row_new ();
    gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
    GtkWidget *label = gtk_label_new (text);
    gtk_widget_add_css_class (label, "result-section");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_set_margin_start (label, 12);
    gtk_widget_set_margin_top (label, 8);
    gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
    return GTK_LIST_BOX_ROW (row);
}

static void
clear_list (MuxSearchWindow *self)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->list))) != NULL)
        gtk_list_box_remove (self->list, child);
}

/* ---------- launch ---------- */

static void
activate_row (MuxSearchWindow *self, GtkListBoxRow *row)
{
    if (row == NULL)
        return;
    const char *kind = g_object_get_data (G_OBJECT (row), "mux-kind");
    if (kind == NULL)
        return; /* section row */
    if (g_strcmp0 (kind, "app") == 0) {
        MuxAppItem *item = g_object_get_data (G_OBJECT (row), "mux-app-item");
        g_autoptr (GError) err = NULL;
        if (item != NULL && !mux_app_item_launch (item, &err))
            g_warning ("mux-launcher: launch ব্যর্থ: %s", err->message);
    } else if (g_strcmp0 (kind, "file") == 0) {
        const char *path = g_object_get_data (G_OBJECT (row), "mux-file-path");
        g_autofree char *uri = g_filename_to_uri (path, NULL, NULL);
        g_autoptr (GError) err = NULL;
        if (uri != NULL &&
            !g_app_info_launch_default_for_uri (uri, NULL, &err))
            g_warning ("mux-launcher: ফাইল খোলা ব্যর্থ: %s", err->message);
    }
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static void
on_row_activated (GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    (void) list;
    activate_row (user_data, row);
}

static void
on_entry_activate (GtkSearchEntry *entry, gpointer user_data)
{
    (void) entry;
    MuxSearchWindow *self = user_data;
    GtkListBoxRow *row = gtk_list_box_get_selected_row (self->list);
    if (row == NULL)
        row = gtk_list_box_get_row_at_index (self->list, 0);
    /* প্রথম row section হলে পরেরটা নাও */
    if (row != NULL &&
        g_object_get_data (G_OBJECT (row), "mux-kind") == NULL)
        row = gtk_list_box_get_row_at_index (
            self->list, gtk_list_box_row_get_index (row) + 1);
    activate_row (self, row);
}

/* ---------- search চালানো ---------- */

typedef struct {
    MuxSearchWindow *self;
    guint stamp;
} FileDoneCtx;

static void
on_file_results (GPtrArray *results, gpointer user_data)
{
    FileDoneCtx *ctx = user_data;
    MuxSearchWindow *self = ctx->self;
    guint stamp = ctx->stamp;
    g_free (ctx);
    if (stamp != self->query_stamp || results->len == 0) {
        g_ptr_array_unref (results);
        return;
    }
    gtk_list_box_append (self->list, GTK_WIDGET (make_section_row ("ফাইল")));
    for (guint i = 0; i < results->len; i++)
        gtk_list_box_append (self->list,
                             GTK_WIDGET (make_file_row (
                                 g_ptr_array_index (results, i))));
    g_ptr_array_unref (results);
}

static gboolean
run_search (gpointer user_data)
{
    MuxSearchWindow *self = user_data;
    self->debounce_id = 0;
    const char *q = gtk_editable_get_text (GTK_EDITABLE (self->entry));
    self->query_stamp++;

    g_cancellable_cancel (self->file_cancel);
    g_clear_object (&self->file_cancel);
    clear_list (self);

    if (mux_search_query_empty (q)) {
        gtk_label_set_text (self->hint, "App বা ফাইলের নাম লিখুন…");
        return G_SOURCE_REMOVE;
    }
    gtk_label_set_text (self->hint, "");

    g_autoptr (GPtrArray) apps = mux_app_model_search (
        self->model, q, self->cfg->search_max_results);
    if (apps->len > 0) {
        gtk_list_box_append (self->list,
                             GTK_WIDGET (make_section_row ("অ্যাপ")));
        for (guint i = 0; i < apps->len; i++)
            gtk_list_box_append (self->list,
                                 GTK_WIDGET (make_app_row (
                                     g_ptr_array_index (apps, i))));
        GtkListBoxRow *first = gtk_list_box_get_row_at_index (self->list, 1);
        if (first != NULL)
            gtk_list_box_select_row (self->list, first);
    }

    if (self->cfg->file_search) {
        self->file_cancel = g_cancellable_new ();
        FileDoneCtx *ctx = g_new0 (FileDoneCtx, 1);
        ctx->self = self;
        ctx->stamp = self->query_stamp;
        mux_file_search_async (q, self->cfg->file_max_depth,
                               self->cfg->file_max_results,
                               self->file_cancel, on_file_results, ctx);
    }
    return G_SOURCE_REMOVE;
}

static void
on_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
    (void) entry;
    MuxSearchWindow *self = user_data;
    if (self->debounce_id != 0)
        g_source_remove (self->debounce_id);
    /* প্রতি অক্ষরে পুরো খোঁজ না — 120ms থেমে চালাও */
    self->debounce_id = g_timeout_add (120, run_search, self);
}

/* ---------- keyboard ---------- */

static void
move_selection (MuxSearchWindow *self, int delta)
{
    GtkListBoxRow *sel = gtk_list_box_get_selected_row (self->list);
    int idx = sel != NULL ? gtk_list_box_row_get_index (sel) : -1;
    for (int step = 0; step < 64; step++) {
        idx += delta;
        if (idx < 0)
            return;
        GtkListBoxRow *row = gtk_list_box_get_row_at_index (self->list, idx);
        if (row == NULL)
            return;
        if (g_object_get_data (G_OBJECT (row), "mux-kind") != NULL) {
            gtk_list_box_select_row (self->list, row);
            return;
        }
    }
}

static gboolean
on_key (GtkEventControllerKey *ctl, guint keyval, guint keycode,
        GdkModifierType state, gpointer user_data)
{
    (void) ctl; (void) keycode; (void) state;
    MuxSearchWindow *self = user_data;
    switch (keyval) {
    case GDK_KEY_Escape:
        gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
        return TRUE;
    case GDK_KEY_Down:
        move_selection (self, +1);
        return TRUE;
    case GDK_KEY_Up:
        move_selection (self, -1);
        return TRUE;
    default:
        return FALSE;
    }
}

/* ---------- GObject ---------- */

static void
mux_search_window_finalize (GObject *obj)
{
    MuxSearchWindow *self = MUX_SEARCH_WINDOW (obj);
    if (self->debounce_id != 0)
        g_source_remove (self->debounce_id);
    g_cancellable_cancel (self->file_cancel);
    g_clear_object (&self->file_cancel);
    g_clear_object (&self->model);
    G_OBJECT_CLASS (mux_search_window_parent_class)->finalize (obj);
}

static void
mux_search_window_class_init (MuxSearchWindowClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = mux_search_window_finalize;
}

static void
mux_search_window_init (MuxSearchWindow *self)
{
    (void) self;
}

MuxSearchWindow *
mux_search_window_new (GtkApplication *app, MuxAppModel *model,
                       MuxLauncherConfig *cfg)
{
    MuxSearchWindow *self = g_object_new (MUX_TYPE_SEARCH_WINDOW,
                                          "application", app, NULL);
    self->model = g_object_ref (model);
    self->cfg = cfg;

    gtk_window_set_title (GTK_WINDOW (self), "Mux Search");
    gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
    gtk_window_set_default_size (GTK_WINDOW (self), 640, -1);
    gtk_widget_add_css_class (GTK_WIDGET (self), "mux-search-window");
    /* পর্দার উপর থেকে ~16% নিচে কার্ডটা ভাসে */
    mux_layer_make_top_panel (GTK_WINDOW (self), "mux-search", 140);

    GtkWidget *card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class (card, "spotlight-card");

    self->entry = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
    gtk_widget_add_css_class (GTK_WIDGET (self->entry), "spotlight-entry");
    /* নোট: GTK4-এ GtkSearchEntry GtkEntry-র subclass না — placeholder
     * API নেই; তার বদলে নিচের hint label-ই সেই কাজ করে। */
    g_signal_connect (self->entry, "search-changed",
                      G_CALLBACK (on_search_changed), self);
    g_signal_connect (self->entry, "activate",
                      G_CALLBACK (on_entry_activate), self);
    gtk_box_append (GTK_BOX (card), GTK_WIDGET (self->entry));

    self->hint = GTK_LABEL (gtk_label_new ("App বা ফাইলের নাম লিখুন…"));
    gtk_widget_add_css_class (GTK_WIDGET (self->hint), "spotlight-hint");
    gtk_box_append (GTK_BOX (card), GTK_WIDGET (self->hint));

    GtkWidget *scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_max_content_height (
        GTK_SCROLLED_WINDOW (scroll), 380);
    gtk_scrolled_window_set_propagate_natural_height (
        GTK_SCROLLED_WINDOW (scroll), TRUE);

    self->list = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_widget_add_css_class (GTK_WIDGET (self->list), "result-list");
    gtk_list_box_set_selection_mode (self->list, GTK_SELECTION_SINGLE);
    g_signal_connect (self->list, "row-activated",
                      G_CALLBACK (on_row_activated), self);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll),
                                   GTK_WIDGET (self->list));
    gtk_box_append (GTK_BOX (card), scroll);

    gtk_window_set_child (GTK_WINDOW (self), card);

    GtkEventController *keys = gtk_event_controller_key_new ();
    g_signal_connect (keys, "key-pressed", G_CALLBACK (on_key), self);
    gtk_widget_add_controller (GTK_WIDGET (self), keys);

    return self;
}

void
mux_search_window_toggle (MuxSearchWindow *self)
{
    if (gtk_widget_get_visible (GTK_WIDGET (self))) {
        gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
    } else {
        gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
        clear_list (self);
        gtk_label_set_text (self->hint, "App বা ফাইলের নাম লিখুন…");
        gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
        gtk_widget_grab_focus (GTK_WIDGET (self->entry));
        /* slide-down: margin 126 → 140 + fade-in */
        mux_animate_top_margin (GTK_WINDOW (self), 126, 140,
                                self->cfg->animation_ms, NULL, NULL);
        mux_animate_opacity (GTK_WIDGET (self), 0.0, 1.0,
                             self->cfg->animation_ms, NULL, NULL);
    }
}
