/* launcher-window.c — দেখো launcher-window.h */
#include "launcher-window.h"
#include "layer-util.h"
#include "animation.h"
#include "search-engine.h"

struct _MuxLauncherWindow {
    GtkWindow parent_instance;
    MuxAppModel *model;          /* ref ধরা */
    MuxLauncherConfig *cfg;      /* borrowed — main-এর মালিকানায় */
    GtkSearchEntry *entry;
    GtkStack *stack;             /* প্রতি page একটা GtkGrid */
    GtkBox *dots;                /* page dot বোতামগুলো */
    int current_page;
    gboolean filtering;          /* search চললে paging বন্ধ */
};

G_DEFINE_TYPE (MuxLauncherWindow, mux_launcher_window, GTK_TYPE_WINDOW)

/* ---------- একটা app-এর ঘর (icon + নাম) ---------- */

static void
on_app_clicked (GtkButton *btn, gpointer user_data)
{
    MuxLauncherWindow *self = user_data;
    MuxAppItem *item = g_object_get_data (G_OBJECT (btn), "mux-app-item");
    if (item == NULL)
        return;
    g_autoptr (GError) err = NULL;
    if (!mux_app_item_launch (item, &err))
        g_warning ("mux-launcher: launch ব্যর্থ (%s): %s",
                   mux_app_item_get_name (item), err->message);
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static GtkWidget *
make_app_cell (MuxLauncherWindow *self, MuxAppItem *item)
{
    GtkWidget *btn = gtk_button_new ();
    gtk_widget_add_css_class (btn, "app-cell");
    g_object_set_data (G_OBJECT (btn), "mux-app-item", item);
    g_signal_connect (btn, "clicked", G_CALLBACK (on_app_clicked), self);

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);

    GIcon *icon = mux_app_item_get_icon (item);
    GtkWidget *img = icon != NULL
        ? gtk_image_new_from_gicon (icon)
        : gtk_image_new_from_icon_name ("application-x-executable");
    gtk_image_set_pixel_size (GTK_IMAGE (img), self->cfg->grid_icon_size);
    gtk_box_append (GTK_BOX (box), img);

    GtkWidget *label = gtk_label_new (mux_app_item_get_name (item));
    gtk_widget_add_css_class (label, "app-name");
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 14);
    gtk_box_append (GTK_BOX (box), label);

    gtk_button_set_child (GTK_BUTTON (btn), box);
    return btn;
}

/* ---------- page গঠন ---------- */

static void
clear_children (GtkWidget *parent)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (parent)) != NULL)
        gtk_widget_unparent (child);
}

static void
go_to_page (MuxLauncherWindow *self, int page)
{
    GtkWidget *first = gtk_widget_get_first_child (GTK_WIDGET (self->stack));
    if (first == NULL)
        return;
    int n = 0;
    for (GtkWidget *c = first; c != NULL; c = gtk_widget_get_next_sibling (c))
        n++;
    page = CLAMP (page, 0, n - 1);

    int i = 0;
    for (GtkWidget *c = first; c != NULL; c = gtk_widget_get_next_sibling (c), i++) {
        if (i == page) {
            self->current_page = page;
            gtk_stack_set_visible_child (self->stack, c);
            break;
        }
    }
    int d = 0;
    for (GtkWidget *b = gtk_widget_get_first_child (GTK_WIDGET (self->dots));
         b != NULL; b = gtk_widget_get_next_sibling (b), d++) {
        if (d == page)
            gtk_widget_add_css_class (b, "active-dot");
        else
            gtk_widget_remove_css_class (b, "active-dot");
    }
}

static void
on_dot_clicked (GtkButton *btn, gpointer user_data)
{
    MuxLauncherWindow *self = user_data;
    int page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (btn), "mux-page"));
    go_to_page (self, page);
}

/* items (MuxAppItem-এর GPtrArray, borrowed) দিয়ে grid আবার বানাও।
 * filtering=TRUE হলে একটাই page (search ফলাফল), নাহলে page ভাগ। */
static void
rebuild_grid (MuxLauncherWindow *self, GPtrArray *items, gboolean filtering)
{
    self->filtering = filtering;
    clear_children (GTK_WIDGET (self->stack));
    clear_children (GTK_WIDGET (self->dots));

    int per_page = self->cfg->grid_columns * self->cfg->grid_rows;
    int pages = filtering ? 1
                          : MAX (1, (int) ((items->len + per_page - 1) / per_page));

    for (int p = 0; p < pages; p++) {
        GtkWidget *grid = gtk_grid_new ();
        gtk_widget_add_css_class (grid, "app-grid");
        gtk_grid_set_row_spacing (GTK_GRID (grid), 28);
        gtk_grid_set_column_spacing (GTK_GRID (grid), 20);
        gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
        gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
        gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

        guint start = filtering ? 0 : (guint) (p * per_page);
        guint end = filtering ? items->len
                              : MIN (items->len, start + (guint) per_page);
        for (guint i = start; i < end; i++) {
            guint k = i - start;
            GtkWidget *cell = make_app_cell (self,
                                             g_ptr_array_index (items, i));
            gtk_grid_attach (GTK_GRID (grid), cell,
                             (int) (k % (guint) self->cfg->grid_columns),
                             (int) (k / (guint) self->cfg->grid_columns), 1, 1);
        }
        gtk_stack_add_child (self->stack, grid);

        if (!filtering && pages > 1) {
            GtkWidget *dot = gtk_button_new ();
            gtk_widget_add_css_class (dot, "page-dot");
            g_object_set_data (G_OBJECT (dot), "mux-page",
                               GINT_TO_POINTER (p));
            g_signal_connect (dot, "clicked", G_CALLBACK (on_dot_clicked), self);
            gtk_box_append (self->dots, dot);
        }
    }
    go_to_page (self, 0);
}

static void
refresh_all (MuxLauncherWindow *self)
{
    const char *q = gtk_editable_get_text (GTK_EDITABLE (self->entry));
    if (mux_search_query_empty (q)) {
        rebuild_grid (self, mux_app_model_get_all (self->model), FALSE);
    } else {
        g_autoptr (GPtrArray) r = mux_app_model_search (
            self->model, q, self->cfg->grid_columns * self->cfg->grid_rows);
        rebuild_grid (self, r, TRUE);
    }
}

static void
on_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
    (void) entry;
    refresh_all (user_data);
}

static void
on_model_changed (MuxAppModel *model, gpointer user_data)
{
    (void) model;
    refresh_all (user_data);
}

/* ---------- keyboard / gesture ---------- */

static gboolean
on_key (GtkEventControllerKey *ctl, guint keyval, guint keycode,
        GdkModifierType state, gpointer user_data)
{
    (void) ctl; (void) keycode; (void) state;
    MuxLauncherWindow *self = user_data;
    switch (keyval) {
    case GDK_KEY_Escape:
        gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
        return TRUE;
    case GDK_KEY_Right:
        if (!self->filtering) {
            go_to_page (self, self->current_page + 1);
            return TRUE;
        }
        break;
    case GDK_KEY_Left:
        if (!self->filtering) {
            go_to_page (self, self->current_page - 1);
            return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

static void
on_swipe (GtkGestureSwipe *gesture, double vx, double vy, gpointer user_data)
{
    (void) gesture; (void) vy;
    MuxLauncherWindow *self = user_data;
    if (self->filtering)
        return;
    if (vx < -0.5)
        go_to_page (self, self->current_page + 1);
    else if (vx > 0.5)
        go_to_page (self, self->current_page - 1);
}

/* ---------- GObject ---------- */

static void
mux_launcher_window_finalize (GObject *obj)
{
    MuxLauncherWindow *self = MUX_LAUNCHER_WINDOW (obj);
    g_clear_object (&self->model);
    G_OBJECT_CLASS (mux_launcher_window_parent_class)->finalize (obj);
}

static void
mux_launcher_window_class_init (MuxLauncherWindowClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = mux_launcher_window_finalize;
}

static void
mux_launcher_window_init (MuxLauncherWindow *self)
{
    (void) self;
}

MuxLauncherWindow *
mux_launcher_window_new (GtkApplication *app, MuxAppModel *model,
                         MuxLauncherConfig *cfg)
{
    MuxLauncherWindow *self = g_object_new (MUX_TYPE_LAUNCHER_WINDOW,
                                            "application", app, NULL);
    self->model = g_object_ref (model);
    self->cfg = cfg;

    gtk_window_set_title (GTK_WINDOW (self), "Mux Launcher");
    gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
    gtk_widget_add_css_class (GTK_WIDGET (self), "mux-launcher");
    mux_layer_make_fullscreen (GTK_WINDOW (self), "mux-launcher");

    GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_margin_top (root, 36);
    gtk_widget_set_margin_bottom (root, 24);
    gtk_widget_set_margin_start (root, 48);
    gtk_widget_set_margin_end (root, 48);

    /* উপরে search box — লিখলেই grid filter (Spotlight window আলাদাও আছে) */
    self->entry = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
    gtk_widget_add_css_class (GTK_WIDGET (self->entry), "launcher-search");
    gtk_widget_set_size_request (GTK_WIDGET (self->entry), 420, -1);
    gtk_widget_set_halign (GTK_WIDGET (self->entry), GTK_ALIGN_CENTER);
    g_signal_connect (self->entry, "search-changed",
                      G_CALLBACK (on_search_changed), self);
    gtk_box_append (GTK_BOX (root), GTK_WIDGET (self->entry));

    self->stack = GTK_STACK (gtk_stack_new ());
    gtk_stack_set_transition_type (self->stack,
                                   GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration (self->stack, 200);
    gtk_widget_set_vexpand (GTK_WIDGET (self->stack), TRUE);
    gtk_box_append (GTK_BOX (root), GTK_WIDGET (self->stack));

    self->dots = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8));
    gtk_widget_set_halign (GTK_WIDGET (self->dots), GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (root), GTK_WIDGET (self->dots));

    gtk_window_set_child (GTK_WINDOW (self), root);

    GtkEventController *keys = gtk_event_controller_key_new ();
    g_signal_connect (keys, "key-pressed", G_CALLBACK (on_key), self);
    gtk_widget_add_controller (GTK_WIDGET (self), keys);

    GtkGesture *swipe = gtk_gesture_swipe_new ();
    g_signal_connect (swipe, "swipe", G_CALLBACK (on_swipe), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (swipe));

    g_signal_connect (model, "changed", G_CALLBACK (on_model_changed), self);
    refresh_all (self);
    return self;
}

void
mux_launcher_window_toggle (MuxLauncherWindow *self)
{
    if (gtk_widget_get_visible (GTK_WIDGET (self))) {
        /* বন্ধ: সঙ্গে সঙ্গে hide — fade-out compositor ছাড়া মসৃণ হয় না,
         * DESIGN.md-তে সৎ নোট আছে। পরের ধাপে layer fade protocol এলে হবে। */
        gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
    } else {
        gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
        refresh_all (self);
        gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
        gtk_widget_grab_focus (GTK_WIDGET (self->entry));
        /* খোলা: fade-in (animation.c) */
        mux_animate_opacity (GTK_WIDGET (self), 0.0, 1.0,
                             self->cfg->animation_ms, NULL, NULL);
    }
}
