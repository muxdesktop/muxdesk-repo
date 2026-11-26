/* main.c — mux-launcher: MuxLike DE-র Launcher + Search (একটাই binary)
 *
 * ব্যবহার:
 *   mux-launcher              → app grid (Launchpad-ধাঁচ) toggle
 *   mux-launcher --grid       → app grid toggle
 *   mux-launcher --search     → Spotlight-ধাঁচের search toggle
 *   mux-launcher --quit       → চলতি instance বন্ধ
 *
 * প্রথম চালালে process বাসিন্দা হয়ে থাকে (g_application_hold) —
 * window লুকানো থাকে, পরের ডাকে সঙ্গে সঙ্গে খোলে। labwc keybind
 * থেকে এই command-ই ডাকা হয় (config/labwc-snippet.md দেখো)।
 * একই mode আবার ডাকলে toggle — window বন্ধ হয়ে যায়।
 */
#include <gtk/gtk.h>

#include "app-model.h"
#include "mux-config.h"
#include "launcher-window.h"
#include "search-window.h"

/* meson -Dmux_datadir দিয়ে দেয়; না থাকলে পাশের style.css খোঁজে */
#ifndef MUX_DATADIR
#define MUX_DATADIR "."
#endif

typedef struct {
    MuxLauncherConfig *cfg;
    MuxAppModel *model;
    MuxLauncherWindow *grid;
    MuxSearchWindow *search;
} AppState;

static void
load_css (void)
{
    g_autoptr (GtkCssProvider) provider = gtk_css_provider_new ();
    const char *env = g_getenv ("MUX_LAUNCHER_CSS");
    g_autofree char *path = env != NULL
        ? g_strdup (env)
        : g_build_filename (MUX_DATADIR, "muxdesk-launcher", "style.css", NULL);
    g_autoptr (GError) err = NULL;
    gtk_css_provider_load_from_path (provider, path, &err);
    if (err != NULL) {
        g_warning ("mux-launcher: style.css পাওয়া গেল না (%s): %s",
                   path, err->message);
        return;
    }
    GdkDisplay *display = gdk_display_get_default ();
    if (display != NULL)
        gtk_style_context_add_provider_for_display (
            display, GTK_STYLE_PROVIDER (provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
on_activate (GtkApplication *app, gpointer user_data)
{
    AppState *st = user_data;
    if (st->grid != NULL)
        return; /* আগেই তৈরি */

    st->cfg = mux_launcher_config_load (NULL);
    st->model = mux_app_model_new ();

    /* icon theme: অনুমোদিত MuxLike — সব icon এখান থেকেই আসবে */
    GtkSettings *settings = gtk_settings_get_default ();
    if (settings != NULL)
        g_object_set (settings, "gtk-icon-theme-name", st->cfg->icon_theme,
                      NULL);

    load_css ();
    st->grid = mux_launcher_window_new (app, st->model, st->cfg);
    st->search = mux_search_window_new (app, st->model, st->cfg);
    /* বাসিন্দা থাকো — window বন্ধ হলেও process মরবে না */
    g_application_hold (G_APPLICATION (app));
}

static int
on_command_line (GtkApplication *app, GApplicationCommandLine *cmdline,
                 gpointer user_data)
{
    AppState *st = user_data;
    gchar **argv = g_application_command_line_get_arguments (cmdline, NULL);
    gboolean want_search = FALSE, want_quit = FALSE;
    for (int i = 1; argv[i] != NULL; i++) {
        if (g_strcmp0 (argv[i], "--search") == 0)
            want_search = TRUE;
        else if (g_strcmp0 (argv[i], "--grid") == 0)
            want_search = FALSE;
        else if (g_strcmp0 (argv[i], "--quit") == 0)
            want_quit = TRUE;
    }
    g_strfreev (argv);

    if (want_quit) {
        g_application_quit (G_APPLICATION (app));
        return 0;
    }
    g_application_activate (G_APPLICATION (app)); /* window তৈরি নিশ্চিত */
    if (want_search) {
        if (gtk_widget_get_visible (GTK_WIDGET (st->grid)))
            gtk_widget_set_visible (GTK_WIDGET (st->grid), FALSE);
        mux_search_window_toggle (st->search);
    } else {
        if (gtk_widget_get_visible (GTK_WIDGET (st->search)))
            gtk_widget_set_visible (GTK_WIDGET (st->search), FALSE);
        mux_launcher_window_toggle (st->grid);
    }
    return 0;
}

static void
on_shutdown (GtkApplication *app, gpointer user_data)
{
    (void) app;
    AppState *st = user_data;
    g_clear_object (&st->model);
    if (st->cfg != NULL)
        mux_launcher_config_free (st->cfg);
}

int
main (int argc, char **argv)
{
    AppState st = { 0 };
    g_autoptr (GtkApplication) app = gtk_application_new (
        "com.muxdesk.Launcher",
        G_APPLICATION_HANDLES_COMMAND_LINE);
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), &st);
    g_signal_connect (app, "command-line", G_CALLBACK (on_command_line), &st);
    g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), &st);
    return g_application_run (G_APPLICATION (app), argc, argv);
}
