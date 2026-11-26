/* test-sort.c — app-model নাম অনুযায়ী সাজায় কিনা যাচাই।
 * meson test XDG_DATA_DIRS=tests/fakeapps বসিয়ে চালায়, যাতে
 * Alpha/Zebra দুটো জানা .desktop পাওয়া যায়। হাতে চালালে:
 *   XDG_DATA_DIRS=$PWD/tests/fakeapps:/usr/share ./test-sort
 */
#include "app-model.h"

int
main (void)
{
    MuxAppModel *m = mux_app_model_new ();
    GPtrArray *all = mux_app_model_get_all (m);
    int ia = -1, iz = -1;
    for (guint i = 0; i < all->len; i++) {
        const char *n = mux_app_item_get_name (g_ptr_array_index (all, i));
        if (g_strcmp0 (n, "Alpha") == 0)
            ia = (int) i;
        if (g_strcmp0 (n, "Zebra") == 0)
            iz = (int) i;
    }
    g_assert_cmpint (ia, >=, 0);
    g_assert_cmpint (iz, >=, 0);
    g_assert_cmpint (ia, <, iz); /* Alpha আগে, Zebra পরে */
    g_object_unref (m);
    g_print ("sort ok: Alpha(%d) < Zebra(%d)\n", ia, iz);
    return 0;
}
