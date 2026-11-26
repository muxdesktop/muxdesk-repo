/* launcher-window.h — Launchpad-ধাঁচের fullscreen app grid
 *
 * আচরণ:
 *  - পুরো পর্দা জুড়ে layer-shell OVERLAY window, পেছনে হালকা গাঢ়/blur-ভাব
 *    (blur আসলে compositor-এর কাজ — style.css-তে স্বচ্ছ গাঢ় রঙ, DESIGN.md দেখো)
 *  - উপরে search box — লিখলেই grid filter হয় (একই search-engine)
 *  - App গুলো page-এ ভাগ (columns×rows), নিচে page-dot, swipe/চাকা/
 *    arrow key দিয়ে page বদল
 *  - Icon MuxLike theme থেকে (config icon-theme), নাম icon-এর নিচে
 *  - Click / Enter-এ app launch হয়ে grid বন্ধ; Esc-এ বন্ধ
 */
#pragma once

#include <gtk/gtk.h>
#include "app-model.h"
#include "mux-config.h"

G_BEGIN_DECLS

#define MUX_TYPE_LAUNCHER_WINDOW (mux_launcher_window_get_type ())
G_DECLARE_FINAL_TYPE (MuxLauncherWindow, mux_launcher_window, MUX,
                      LAUNCHER_WINDOW, GtkWindow)

MuxLauncherWindow *mux_launcher_window_new (GtkApplication *app,
                                            MuxAppModel *model,
                                            MuxLauncherConfig *cfg);
void mux_launcher_window_toggle (MuxLauncherWindow *self);

G_END_DECLS
