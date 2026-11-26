/* mux-config.h — mux-launcher-এর config (GKeyFile / INI)
 *
 * MuxLike DE — Launcher ও Search component
 * ফাইল: ~/.config/muxdesk/mux-launcher.ini  (না থাকলে default মান)
 * এই header/code GTK-মুক্ত — শুধু GLib, তাই আলাদা test করা যায়।
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
    /* [general] */
    char *icon_theme;        /* default "MuxLike" — অনুমোদিত theme */
    int   animation_ms;      /* খোলা/বন্ধ animation, default 180, 0 = বন্ধ */
    /* [grid] — Launchpad grid */
    int   grid_columns;      /* default 7 */
    int   grid_rows;         /* default 4  → প্রতি page-এ 28টা app */
    int   grid_icon_size;    /* default 96 px */
    /* [search] — Spotlight-ধাঁচ */
    int   search_max_results;   /* default 8 */
    gboolean file_search;       /* file খোঁজার hook চালু? default TRUE */
    int   file_max_depth;       /* $HOME-এর নিচে কত গভীর, default 3 */
    int   file_max_results;     /* default 5 */
} MuxLauncherConfig;

/* config লোড করে (path NULL হলে default পথ)। ভুল থাকলে default-এ ফেরে,
 * কখনো crash করে না। মালিকানা caller-এর — mux_launcher_config_free() ডাকতে হবে। */
MuxLauncherConfig *mux_launcher_config_load (const char *path);
void               mux_launcher_config_free (MuxLauncherConfig *cfg);

/* default config পথ: $XDG_CONFIG_HOME/muxdesk/mux-launcher.ini */
char *mux_launcher_config_default_path (void);

G_END_DECLS
