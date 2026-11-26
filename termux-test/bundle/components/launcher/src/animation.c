/* animation.c — দেখো animation.h
 * বাস্তবায়ন: g_timeout (প্রতি ~16ms = 60fps ধাপ) + ease-out cubic।
 * Frame clock-ভিত্তিক করা যায় পরে; ফোনের software rendering-এ
 * এই হালকা পথটাই যথেষ্ট ও নিরাপদ।
 */
#include "animation.h"

#include <gtk4-layer-shell.h>
#include <math.h>

typedef struct {
    GtkWidget *widget;      /* ref ধরা */
    GtkWindow *window;      /* margin mode-এ */
    double from, to;
    int from_px, to_px;
    int ms;
    gint64 start;           /* monotonic µs */
    gboolean margin_mode;
    MuxAnimDone done;
    gpointer user_data;
} Anim;

static double
ease_out_cubic (double t)
{
    double u = 1.0 - t;
    return 1.0 - u * u * u;
}

static void
anim_finish (Anim *a)
{
    if (a->margin_mode) {
        gtk_layer_set_margin (a->window, GTK_LAYER_SHELL_EDGE_TOP, a->to_px);
        g_object_unref (a->window);
    } else {
        gtk_widget_set_opacity (a->widget, a->to);
        g_object_unref (a->widget);
    }
    if (a->done != NULL)
        a->done (a->user_data);
    g_free (a);
}

static gboolean
anim_tick (gpointer data)
{
    Anim *a = data;
    gint64 now = g_get_monotonic_time ();
    double t = a->ms > 0 ? (double) (now - a->start) / (a->ms * 1000.0) : 1.0;
    if (t >= 1.0) {
        anim_finish (a);
        return G_SOURCE_REMOVE;
    }
    double e = ease_out_cubic (t);
    if (a->margin_mode) {
        int v = (int) lround (a->from_px + (a->to_px - a->from_px) * e);
        gtk_layer_set_margin (a->window, GTK_LAYER_SHELL_EDGE_TOP, v);
    } else {
        gtk_widget_set_opacity (a->widget, a->from + (a->to - a->from) * e);
    }
    return G_SOURCE_CONTINUE;
}

static void
anim_start (Anim *a)
{
    a->start = g_get_monotonic_time ();
    if (a->ms <= 0) {
        anim_finish (a);
        return;
    }
    g_timeout_add (16, anim_tick, a);
}

void
mux_animate_opacity (GtkWidget *widget, double from, double to,
                     int ms, MuxAnimDone done, gpointer user_data)
{
    Anim *a = g_new0 (Anim, 1);
    a->widget = g_object_ref (widget);
    a->from = from; a->to = to; a->ms = ms;
    a->done = done; a->user_data = user_data;
    gtk_widget_set_opacity (widget, from);
    anim_start (a);
}

void
mux_animate_top_margin (GtkWindow *window, int from, int to,
                        int ms, MuxAnimDone done, gpointer user_data)
{
    if (!gtk_layer_is_layer_window (window)) {
        if (done != NULL)
            done (user_data);
        return;
    }
    Anim *a = g_new0 (Anim, 1);
    a->window = g_object_ref (window);
    a->margin_mode = TRUE;
    a->from_px = from; a->to_px = to; a->ms = ms;
    a->done = done; a->user_data = user_data;
    gtk_layer_set_margin (window, GTK_LAYER_SHELL_EDGE_TOP, from);
    anim_start (a);
}
