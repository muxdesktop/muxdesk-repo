/* test-core.c — GTK-মুক্ত অংশের পরীক্ষা: search score, config, file search
 * চালানো: meson test  (বা হাতে: নিচে README দেখো)
 */
#include "search-engine.h"
#include "mux-config.h"
#include "app-model.h"

#include <string.h>
#include <glib/gstdio.h>

static void
test_score_exact_and_prefix (void)
{
    g_assert_cmpint (mux_search_score ("Files", "Files", NULL), >, 900);
    g_assert_cmpint (mux_search_score ("fil", "Files", NULL), >, 700);
    g_assert_cmpint (mux_search_score ("files", "FILES", NULL), >, 900); /* case */
}

static void
test_score_word_and_substring (void)
{
    int word = mux_search_score ("term", "Mux Terminal", NULL);
    int sub  = mux_search_score ("mina", "Mux Terminal", NULL);
    g_assert_cmpint (word, >, sub);
    g_assert_cmpint (sub, >, 0);
}

static void
test_score_subsequence_and_miss (void)
{
    g_assert_cmpint (mux_search_score ("trm", "Terminal", NULL), >, 0);
    g_assert_cmpint (mux_search_score ("zzzz", "Terminal", NULL), ==, -1);
}

static void
test_score_keyword (void)
{
    const char *kw[] = { "shell", "console", NULL };
    g_assert_cmpint (mux_search_score ("shell", "Terminal", kw), >, 0);
}

static void
test_score_bengali (void)
{
    /* বাংলা নামেও prefix মিল কাজ করা চাই */
    g_assert_cmpint (mux_search_score ("ফাই", "ফাইল", NULL), >, 700);
}

static void
test_config_defaults (void)
{
    MuxLauncherConfig *cfg = mux_launcher_config_load ("/nonexistent/mux.ini");
    g_assert_cmpstr (cfg->icon_theme, ==, "MuxLike");
    g_assert_cmpint (cfg->grid_columns, ==, 7);
    g_assert_cmpint (cfg->grid_icon_size, ==, 96);
    g_assert_true (cfg->file_search);
    mux_launcher_config_free (cfg);
}

static void
test_model_loads (void)
{
    /* এই machine-এ .desktop থাকুক বা না থাকুক — crash করা যাবে না,
     * আর search খালি query-তে get_all-এর সমান ফল দেবে */
    MuxAppModel *m = mux_app_model_new ();
    GPtrArray *all = mux_app_model_get_all (m);
    g_autoptr (GPtrArray) r = mux_app_model_search (m, "", 0);
    g_assert_cmpuint (r->len, ==, all->len);
    g_autoptr (GPtrArray) none = mux_app_model_search (m, "zzzzqqqq-nope", 0);
    g_assert_cmpuint (none->len, ==, 0);
    g_object_unref (m);
}

typedef struct { GMainLoop *loop; GPtrArray *results; } FileTest;

static void
on_files (GPtrArray *results, gpointer data)
{
    FileTest *t = data;
    t->results = results;
    g_main_loop_quit (t->loop);
}

static void
test_file_search (void)
{
    /* নিজের HOME-এ একটা জানা ফাইল বানিয়ে খোঁজা */
    g_autofree char *name = g_strdup ("mux-launcher-testfile-xyz.txt");
    g_autofree char *path = g_build_filename (g_get_home_dir (), name, NULL);
    g_assert_true (g_file_set_contents (path, "hi", -1, NULL));

    FileTest t = { g_main_loop_new (NULL, FALSE), NULL };
    mux_file_search_async ("mux-launcher-testfile", 1, 5, NULL, on_files, &t);
    g_main_loop_run (t.loop);

    gboolean found = FALSE;
    for (guint i = 0; i < t.results->len; i++) {
        MuxFileResult *r = g_ptr_array_index (t.results, i);
        if (strstr (mux_file_result_get_path (r), "mux-launcher-testfile") != NULL)
            found = TRUE;
    }
    g_assert_true (found);
    g_ptr_array_unref (t.results);
    g_main_loop_unref (t.loop);
    g_unlink (path);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/score/exact-prefix", test_score_exact_and_prefix);
    g_test_add_func ("/score/word-substring", test_score_word_and_substring);
    g_test_add_func ("/score/subsequence-miss", test_score_subsequence_and_miss);
    g_test_add_func ("/score/keyword", test_score_keyword);
    g_test_add_func ("/score/bengali", test_score_bengali);
    g_test_add_func ("/config/defaults", test_config_defaults);
    g_test_add_func ("/model/loads", test_model_loads);
    g_test_add_func ("/file-search/finds", test_file_search);
    return g_test_run ();
}
