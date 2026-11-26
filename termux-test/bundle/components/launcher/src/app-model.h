/* app-model.h — .desktop file পড়ে app-এর তালিকা (GTK-মুক্ত, GLib/GIO)
 *
 * GAppInfo / GDesktopAppInfo-ই আসল কাজটা করে:
 *  - $XDG_DATA_HOME/applications, /usr/share/applications,
 *    /usr/local/share/applications ... সব standard জায়গা পড়ে
 *  - NoDisplay / Hidden / OnlyShowIn সম্মান করে (g_app_info_should_show)
 *  - Icon, নাম, keyword, Exec — সব .desktop থেকেই আসে
 *
 * MuxAppItem একটা app; MuxAppModel তালিকা ধরে রাখে, নাম ধরে সাজায়,
 * search query দিয়ে score-সহ ফলাফল দেয়, আর applications folder
 * বদলালে "changed" signal দেয় (নতুন app বসলে grid নিজে refresh হবে)।
 */
#pragma once

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

G_BEGIN_DECLS

/* ---------- একটা app ---------- */

#define MUX_TYPE_APP_ITEM (mux_app_item_get_type ())
G_DECLARE_FINAL_TYPE (MuxAppItem, mux_app_item, MUX, APP_ITEM, GObject)

MuxAppItem     *mux_app_item_new (GDesktopAppInfo *info);
GDesktopAppInfo *mux_app_item_get_info (MuxAppItem *self);
const char     *mux_app_item_get_name (MuxAppItem *self);       /* প্রদর্শনের নাম */
const char     *mux_app_item_get_id (MuxAppItem *self);         /* যেমন "mux-files.desktop" */
GIcon          *mux_app_item_get_icon (MuxAppItem *self);       /* NULL হতে পারে */
const char *const *mux_app_item_get_keywords (MuxAppItem *self);
/* query দিয়ে launch করা; ব্যর্থ হলে FALSE + err */
gboolean        mux_app_item_launch (MuxAppItem *self, GError **err);

/* ---------- পুরো তালিকা ---------- */

#define MUX_TYPE_APP_MODEL (mux_app_model_get_type ())
G_DECLARE_FINAL_TYPE (MuxAppModel, mux_app_model, MUX, APP_MODEL, GObject)

MuxAppModel *mux_app_model_new (void);

/* সব app, নাম অনুযায়ী সাজানো। GPtrArray<MuxAppItem> (model-এর মালিকানায়,
 * free করতে হবে না — পরের reload-এ বদলে যেতে পারে)। */
GPtrArray *mux_app_model_get_all (MuxAppModel *self);

/* query দিয়ে খোঁজা: মেলা app গুলো score-এর ক্রমে (ভালো মিল আগে)।
 * ফলাফল caller-এর — g_ptr_array_unref() (ভিতরের item model-এরই থাকে)।
 * limit <= 0 মানে সীমা নেই। খালি query হলে সব app নামের ক্রমে। */
GPtrArray *mux_app_model_search (MuxAppModel *self, const char *query, int limit);

/* তালিকা আবার পড়ো (নতুন .desktop এলে)। "changed" signal-ও আসবে। */
void mux_app_model_reload (MuxAppModel *self);

/* signals: "changed" — তালিকা বদলেছে (folder monitor বা reload) */

G_END_DECLS
