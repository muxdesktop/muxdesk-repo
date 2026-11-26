/* search-engine.h — Spotlight-ধাঁচের search-এর মাথা (GTK-মুক্ত, শুধু GLib/GIO)
 *
 * দুটো কাজ:
 *  1. App মিলানো: query দিয়ে app-এর নাম/keyword-এ score দেওয়া
 *     (mux_search_score) — বাংলা/Unicode নামেও চলে (g_utf8_casefold)।
 *  2. File খোঁজার hook: MuxFileSearcher — $HOME-এর নিচে নাম মিলিয়ে
 *     ফাইল খোঁজে, async + cancellable, যাতে UI আটকে না যায়।
 *     এটা একটা "hook" — পরে tracker/miner বা নিজের index এলে এই
 *     interface-এর পেছনে বসবে, UI বদলাতে হবে না।
 */
#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/* ---------- 1. App নাম মেলানো ---------- */

/* Score: -1 = মেলে না। বেশি = ভালো মিল।
 *   হুবহু নাম        1000
 *   নামের শুরুতে     800
 *   কোনো শব্দের শুরু  600
 *   নামের ভিতরে      400
 *   keyword-এ মিল    350
 *   subsequence      200   (যেমন "trm" → "Terminal")
 * সব score থেকে নামের দৈর্ঘ্যের ছোট জরিমানা বাদ যায়, যাতে
 * ছোট সঠিক নাম লম্বা নামের ওপরে থাকে। */
int mux_search_score (const char *query, const char *name,
                      const char *const *keywords);

/* query খালি হলে TRUE (সব দেখাও — grid-এর default অবস্থা) */
gboolean mux_search_query_empty (const char *query);

/* ---------- 2. File search hook ---------- */

#define MUX_TYPE_FILE_RESULT (mux_file_result_get_type ())
G_DECLARE_FINAL_TYPE (MuxFileResult, mux_file_result, MUX, FILE_RESULT, GObject)

const char *mux_file_result_get_path        (MuxFileResult *self);
const char *mux_file_result_get_display_name (MuxFileResult *self);
GIcon      *mux_file_result_get_icon        (MuxFileResult *self);

/* callback: results = GPtrArray<MuxFileResult> (transfer full) */
typedef void (*MuxFileSearchDone) (GPtrArray *results, gpointer user_data);

/* $HOME-এর নিচে basename-এ query (substring, case-insensitive) মেলে
 * এমন ফাইল খোঁজে। লুকানো folder (.cache ইত্যাদি) ও max_depth-এর
 * বাইরে যায় না। কাজটা worker thread-এ হয়; done main context-এ ডাকে।
 * cancellable বাতিল করলে done খালি তালিকা নিয়ে ডাকবে। */
void mux_file_search_async (const char *query,
                            int max_depth,
                            int max_results,
                            GCancellable *cancellable,
                            MuxFileSearchDone done,
                            gpointer user_data);

G_END_DECLS
