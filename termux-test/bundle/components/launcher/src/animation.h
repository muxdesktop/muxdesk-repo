/* animation.h — খোলা/বন্ধের হালকা animation (fade + slide)
 *
 * পরিকল্পনা (DESIGN.md-তে বিস্তারিত):
 *  - Grid: backdrop fade-in + icon grid-এর opacity 0→1, 180ms ease-out
 *  - Search: কার্ড উপর থেকে 14px নেমে আসে (layer margin) + fade
 *  - বন্ধ: উল্টোটা, তারপর আসল hide
 * আসল scale/zoom (Launchpad-এর মতো icon বড় হওয়া) GTK4-এ widget
 * snapshot transform ছাড়া হয় না — সেটা DESIGN.md-তে পরের ধাপ হিসেবে
 * রাখা আছে, এখানে ভান করা হয়নি।
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*MuxAnimDone) (gpointer user_data);

/* widget-এর opacity from→to, ms সময়ে (ease-out cubic)।
 * শেষ হলে done ডাকে (NULL হতে পারে)। ms=0 হলে সঙ্গে সঙ্গে শেষ মান। */
void mux_animate_opacity (GtkWidget *widget, double from, double to,
                          int ms, MuxAnimDone done, gpointer user_data);

/* layer-shell window-এর top margin from→to animate করে (slide)।
 * window layer-shell না হলে কিছু করে না, শুধু done ডাকে। */
void mux_animate_top_margin (GtkWindow *window, int from, int to,
                             int ms, MuxAnimDone done, gpointer user_data);

G_END_DECLS
