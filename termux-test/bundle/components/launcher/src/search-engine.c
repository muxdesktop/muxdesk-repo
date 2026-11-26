/* search-engine.c — দেখো search-engine.h */
#include "search-engine.h"

#include <string.h>

/* ================= App নাম মেলানো ================= */

gboolean
mux_search_query_empty (const char *query)
{
    if (query == NULL)
        return TRUE;
    g_autofree char *t = g_strstrip (g_strdup (query));
    return t[0] == '\0';
}

/* casefold + strip করা copy; caller free করবে */
static char *
fold (const char *s)
{
    if (s == NULL)
        return g_strdup ("");
    g_autofree char *t = g_strstrip (g_strdup (s));
    return g_utf8_casefold (t, -1);
}

/* needle কি haystack-এর subsequence? (অক্ষরগুলো পরপর না হলেও সাজানো থাকে) */
static gboolean
is_subsequence (const char *needle, const char *haystack)
{
    const char *n = needle, *h = haystack;
    while (*n != '\0' && *h != '\0') {
        gunichar nc = g_utf8_get_char (n);
        gunichar hc = g_utf8_get_char (h);
        if (nc == hc)
            n = g_utf8_next_char (n);
        h = g_utf8_next_char (h);
    }
    return *n == '\0';
}

/* কোনো শব্দের (space/-/_ দিয়ে ভাগ) শুরুতে needle আছে কিনা */
static gboolean
word_prefix_match (const char *needle, const char *haystack)
{
    const char *p = haystack;
    while (p != NULL && *p != '\0') {
        if (g_str_has_prefix (p, needle))
            return TRUE;
        p = strpbrk (p, " -_");
        if (p != NULL)
            p++;
    }
    return FALSE;
}

int
mux_search_score (const char *query, const char *name,
                  const char *const *keywords)
{
    if (mux_search_query_empty (query))
        return 0; /* খালি query — filter না, score অর্থহীন */

    g_autofree char *q = fold (query);
    g_autofree char *n = fold (name);
    if (q[0] == '\0' || n[0] == '\0')
        return -1;

    int score = -1;
    if (g_strcmp0 (q, n) == 0)
        score = 1000;
    else if (g_str_has_prefix (n, q))
        score = 800;
    else if (word_prefix_match (q, n))
        score = 600;
    else if (strstr (n, q) != NULL)
        score = 400;
    else if (is_subsequence (q, n))
        score = 200;

    /* নামে না মিললে keyword দেখো */
    if (score < 0 && keywords != NULL) {
        for (int i = 0; keywords[i] != NULL; i++) {
            g_autofree char *k = fold (keywords[i]);
            if (strstr (k, q) != NULL || g_str_has_prefix (k, q)) {
                score = 350;
                break;
            }
        }
    }
    if (score < 0)
        return -1;

    /* লম্বা নামের ছোট জরিমানা: প্রতি অক্ষরে 1, সর্বোচ্চ 50 */
    long len = g_utf8_strlen (n, -1);
    score -= (int) MIN (len, 50);
    return score;
}

/* ================= File result ================= */

struct _MuxFileResult {
    GObject parent_instance;
    char *path;
    char *display_name;
    GIcon *icon;
};

G_DEFINE_TYPE (MuxFileResult, mux_file_result, G_TYPE_OBJECT)

static void
mux_file_result_finalize (GObject *obj)
{
    MuxFileResult *self = MUX_FILE_RESULT (obj);
    g_free (self->path);
    g_free (self->display_name);
    g_clear_object (&self->icon);
    G_OBJECT_CLASS (mux_file_result_parent_class)->finalize (obj);
}

static void
mux_file_result_class_init (MuxFileResultClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = mux_file_result_finalize;
}

static void
mux_file_result_init (MuxFileResult *self) { (void) self; }

static MuxFileResult *
mux_file_result_new (const char *path, GFileInfo *info)
{
    MuxFileResult *r = g_object_new (MUX_TYPE_FILE_RESULT, NULL);
    r->path = g_strdup (path);
    const char *dn = g_file_info_get_display_name (info);
    r->display_name = g_strdup (dn != NULL ? dn : g_path_get_basename (path));
    GIcon *icon = g_file_info_get_icon (info);
    r->icon = icon != NULL ? g_object_ref (icon) : NULL;
    return r;
}

const char *mux_file_result_get_path (MuxFileResult *self)
{ return self->path; }
const char *mux_file_result_get_display_name (MuxFileResult *self)
{ return self->display_name; }
GIcon *mux_file_result_get_icon (MuxFileResult *self)
{ return self->icon; }

/* ================= File search (worker thread) ================= */

typedef struct {
    char *query_folded;
    int max_depth;
    int max_results;
    GCancellable *cancellable; /* ref ধরা */
    MuxFileSearchDone done;
    gpointer user_data;
    GPtrArray *results;        /* thread-এ ভরে, main-এ হস্তান্তর */
} FileSearchJob;

static void
walk_dir (FileSearchJob *job, GFile *dir, int depth)
{
    if (depth > job->max_depth)
        return;
    if (g_cancellable_is_cancelled (job->cancellable))
        return;
    if ((int) job->results->len >= job->max_results)
        return;

    g_autoptr (GError) err = NULL;
    g_autoptr (GFileEnumerator) en = g_file_enumerate_children (
        dir, G_FILE_ATTRIBUTE_STANDARD_NAME ","
             G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
             G_FILE_ATTRIBUTE_STANDARD_ICON ","
             G_FILE_ATTRIBUTE_STANDARD_TYPE,
        G_FILE_QUERY_INFO_NONE, job->cancellable, &err);
    if (en == NULL)
        return; /* permission নেই ইত্যাদি — চুপচাপ এড়িয়ে যাও */

    GFileInfo *info;
    while ((info = g_file_enumerator_next_file (en, job->cancellable, NULL)) != NULL) {
        const char *base = g_file_info_get_name (info);
        /* লুকানো ফাইল/folder এড়াও (.cache, .config-এর ভিড় চাই না) */
        if (base != NULL && base[0] != '.') {
            g_autoptr (GFile) child = g_file_get_child (dir, base);
            if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
                walk_dir (job, child, depth + 1);
            } else {
                g_autofree char *fb = fold (base);
                if (strstr (fb, job->query_folded) != NULL) {
                    g_autofree char *path = g_file_get_path (child);
                    g_ptr_array_add (job->results, mux_file_result_new (path, info));
                }
            }
        }
        g_object_unref (info);
        if (g_cancellable_is_cancelled (job->cancellable) ||
            (int) job->results->len >= job->max_results)
            break;
    }
}

static gboolean
finish_on_main (gpointer data)
{
    FileSearchJob *job = data;
    job->done (job->results, job->user_data); /* results-এর মালিকানা callback-এর */
    g_free (job->query_folded);
    g_clear_object (&job->cancellable);
    g_free (job);
    return G_SOURCE_REMOVE;
}

static gpointer
search_thread (gpointer data)
{
    FileSearchJob *job = data;
    g_autoptr (GFile) home = g_file_new_for_path (g_get_home_dir ());
    walk_dir (job, home, 0);
    /* ফলাফল main context-এ পৌঁছে দাও — UI শুধু ওখানেই ছোঁয়া যাবে */
    g_main_context_invoke (NULL, finish_on_main, job);
    return NULL;
}

void
mux_file_search_async (const char *query, int max_depth, int max_results,
                       GCancellable *cancellable,
                       MuxFileSearchDone done, gpointer user_data)
{
    FileSearchJob *job = g_new0 (FileSearchJob, 1);
    job->query_folded = fold (query);
    job->max_depth = MAX (max_depth, 0);
    job->max_results = MAX (max_results, 1);
    job->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;
    job->done = done;
    job->user_data = user_data;
    job->results = g_ptr_array_new_with_free_func (g_object_unref);
    g_thread_unref (g_thread_new ("mux-file-search", search_thread, job));
}
