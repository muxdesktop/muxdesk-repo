/* search-window.h — Spotlight-ধাঁচের search (একই component-এর অংশ)
 *
 * আচরণ:
 *  - পর্দার উপরে মাঝখানে ভাসমান গোল কার্ড (layer-shell OVERLAY panel)
 *  - বড় search box; লিখলেই app ফলাফল সঙ্গে সঙ্গে, ফাইল ফলাফল
 *    async hook থেকে এসে নিচে যোগ হয় (UI আটকায় না)
 *  - ↑/↓ দিয়ে ফলাফল বাছাই, Enter-এ launch (app হলে app, ফাইল হলে
 *    default handler দিয়ে খোলে), Esc-এ বন্ধ
 *  - কিছু না লিখলে সাম্প্রতিক/সব app-এর প্রথম কয়েকটা দেখায় না —
 *    খালি অবস্থায় শুধু ইঙ্গিত লেখা থাকে (Spotlight-ও এমনই)
 */
#pragma once

#include <gtk/gtk.h>
#include "app-model.h"
#include "mux-config.h"

G_BEGIN_DECLS

#define MUX_TYPE_SEARCH_WINDOW (mux_search_window_get_type ())
G_DECLARE_FINAL_TYPE (MuxSearchWindow, mux_search_window, MUX,
                      SEARCH_WINDOW, GtkWindow)

MuxSearchWindow *mux_search_window_new (GtkApplication *app,
                                        MuxAppModel *model,
                                        MuxLauncherConfig *cfg);
void mux_search_window_toggle (MuxSearchWindow *self);

G_END_DECLS
