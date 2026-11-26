/* layer-util.c — দেখো layer-util.h
 * নির্ভরতা: gtk4-layer-shell-0 (Arch: gtk-layer-shell / gtk4-layer-shell)
 * labwc/wlroots দুটোতেই wlr-layer-shell চলে — MuxDesktop-এর নিজের
 * Wayland server-এও একই protocol লাগবে (integration নোট README-তে)।
 */
#include "layer-util.h"

#include <gtk4-layer-shell.h>

void
mux_layer_make_fullscreen (GtkWindow *window, const char *ns)
{
    gtk_layer_init_for_window (window);
    gtk_layer_set_namespace (window, ns);
    gtk_layer_set_layer (window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor (window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor (window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor (window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor (window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    /* -1 = পুরো পর্দা জুড়ে, panel/dock-এর জায়গাও ঢেকে (Launchpad এমনই) */
    gtk_layer_set_exclusive_zone (window, -1);
    gtk_layer_set_keyboard_mode (window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
}

void
mux_layer_make_top_panel (GtkWindow *window, const char *ns, int margin_top)
{
    gtk_layer_init_for_window (window);
    gtk_layer_set_namespace (window, ns);
    gtk_layer_set_layer (window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor (window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    /* LEFT/RIGHT anchor নেই — window নিজের width-এ মাঝখানে থাকে */
    gtk_layer_set_margin (window, GTK_LAYER_SHELL_EDGE_TOP, margin_top);
    gtk_layer_set_exclusive_zone (window, 0);
    gtk_layer_set_keyboard_mode (window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
}
