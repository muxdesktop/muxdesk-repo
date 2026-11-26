/* mux-config.c — দেখো mux-config.h */
#include "mux-config.h"

char *
mux_launcher_config_default_path (void)
{
    return g_build_filename (g_get_user_config_dir (), "muxdesk",
                             "mux-launcher.ini", NULL);
}

static int
get_int (GKeyFile *kf, const char *group, const char *key, int fallback)
{
    GError *err = NULL;
    int v = g_key_file_get_integer (kf, group, key, &err);
    if (err != NULL) {
        g_error_free (err);
        return fallback;
    }
    return v;
}

static gboolean
get_bool (GKeyFile *kf, const char *group, const char *key, gboolean fallback)
{
    GError *err = NULL;
    gboolean v = g_key_file_get_boolean (kf, group, key, &err);
    if (err != NULL) {
        g_error_free (err);
        return fallback;
    }
    return v;
}

MuxLauncherConfig *
mux_launcher_config_load (const char *path)
{
    MuxLauncherConfig *cfg = g_new0 (MuxLauncherConfig, 1);

    /* --- default মান (config না থাকলেও এগুলোই চলবে) --- */
    cfg->icon_theme         = g_strdup ("MuxLike");
    cfg->animation_ms       = 180;
    cfg->grid_columns       = 7;
    cfg->grid_rows          = 4;
    cfg->grid_icon_size     = 96;
    cfg->search_max_results = 8;
    cfg->file_search        = TRUE;
    cfg->file_max_depth     = 3;
    cfg->file_max_results   = 5;

    g_autofree char *owned_path = NULL;
    if (path == NULL) {
        owned_path = mux_launcher_config_default_path ();
        path = owned_path;
    }

    g_autoptr (GKeyFile) kf = g_key_file_new ();
    GError *err = NULL;
    if (!g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, &err)) {
        g_debug ("mux-launcher: config পড়া গেল না (%s): %s — default চলছে",
                 path, err->message);
        g_error_free (err);
        return cfg;
    }

    g_free (cfg->icon_theme);
    cfg->icon_theme = g_key_file_get_string (kf, "general", "icon-theme", NULL);
    if (cfg->icon_theme == NULL)
        cfg->icon_theme = g_strdup ("MuxLike");
    cfg->animation_ms = get_int (kf, "general", "animation-ms", cfg->animation_ms);

    cfg->grid_columns    = get_int (kf, "grid", "columns",   cfg->grid_columns);
    cfg->grid_rows       = get_int (kf, "grid", "rows",      cfg->grid_rows);
    cfg->grid_icon_size  = get_int (kf, "grid", "icon-size", cfg->grid_icon_size);

    cfg->search_max_results = get_int (kf, "search", "max-results",     cfg->search_max_results);
    cfg->file_search        = get_bool (kf, "search", "file-search",    cfg->file_search);
    cfg->file_max_depth     = get_int (kf, "search", "file-max-depth",  cfg->file_max_depth);
    cfg->file_max_results   = get_int (kf, "search", "file-max-results", cfg->file_max_results);

    /* অস্বাভাবিক মান আটকাও — grid ভাঙবে না */
    cfg->grid_columns   = CLAMP (cfg->grid_columns, 3, 12);
    cfg->grid_rows      = CLAMP (cfg->grid_rows, 2, 8);
    cfg->grid_icon_size = CLAMP (cfg->grid_icon_size, 48, 128);
    cfg->animation_ms   = CLAMP (cfg->animation_ms, 0, 600);
    return cfg;
}

void
mux_launcher_config_free (MuxLauncherConfig *cfg)
{
    if (cfg == NULL)
        return;
    g_free (cfg->icon_theme);
    g_free (cfg);
}
