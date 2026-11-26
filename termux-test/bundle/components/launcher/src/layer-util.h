/* layer-util.h — wlroots/labwc layer-shell বসানোর ছোট helper
 * gtk4-layer-shell ব্যবহার করে। দুটো window-ই এখান থেকে বসে:
 *  - grid: fullscreen OVERLAY (সব ধারে anchor, keyboard ধরে)
 *  - search: উপরে মাঝখানে ভাসমান panel (OVERLAY)
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* fullscreen layer window: namespace আলাদা রাখা হয় যাতে labwc
 * rule / অন্য component চিনতে পারে। */
void mux_layer_make_fullscreen (GtkWindow *window, const char *ns);

/* উপরে-মাঝে ভাসা panel: top ধারে anchor, margin_top px নিচে */
void mux_layer_make_top_panel (GtkWindow *window, const char *ns, int margin_top);

G_END_DECLS
