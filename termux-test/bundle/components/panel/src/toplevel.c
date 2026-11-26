#define _GNU_SOURCE
#include "toplevel.h"

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    struct MuxToplevels *owner;
    char *app_id;
    bool activated;
} Top;

struct MuxToplevels {
    struct zwlr_foreign_toplevel_manager_v1 *manager;
    GPtrArray *tops; /* Top* */
    void (*on_change)(void *data);
    void *data;
};

static void notify(MuxToplevels *tl)
{
    if (tl->on_change)
        tl->on_change(tl->data);
}

static void top_free(Top *t)
{
    if (t->handle)
        zwlr_foreign_toplevel_handle_v1_destroy(t->handle);
    g_free(t->app_id);
    g_free(t);
}

static void handle_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
                         const char *title)
{
    (void)data; (void)h; (void)title; /* panel shows app_id, not titles */
}
static void handle_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
                          const char *app_id)
{
    (void)h;
    Top *t = data;
    g_free(t->app_id);
    t->app_id = g_strdup(app_id);
}
static void handle_output(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
                          struct wl_output *output)
{
    (void)data; (void)h; (void)output;
}
static void handle_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *h,
                         struct wl_array *state)
{
    (void)h;
    Top *t = data;
    t->activated = false;
    uint32_t *s;
    wl_array_for_each(s, state) {
        if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            t->activated = true;
    }
}
static void handle_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    (void)h;
    Top *t = data;
    /* A batch of changes for this handle is complete — let the panel
     * re-read the active app and redraw. */
    notify(t->owner);
}
static void handle_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *h)
{
    (void)h;
    Top *t = data;
    MuxToplevels *tl = t->owner;
    g_ptr_array_remove(tl->tops, t); /* frees t via the array's free func */
    notify(tl);
}

static const struct zwlr_foreign_toplevel_handle_v1_listener handle_listener = {
    .title = handle_title,
    .app_id = handle_app_id,
    .output_enter = handle_output,
    .output_leave = handle_output,
    .state = handle_state,
    .done = handle_done,
    .closed = handle_closed,
};

/* --- manager --- */

static void manager_toplevel(void *data,
                             struct zwlr_foreign_toplevel_manager_v1 *mgr,
                             struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)mgr;
    MuxToplevels *tl = data;
    Top *t = g_new0(Top, 1);
    t->handle = handle;
    t->owner = tl;
    g_ptr_array_add(tl->tops, t);
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &handle_listener, t);
    notify(tl);
}
static void manager_finished(void *data,
                             struct zwlr_foreign_toplevel_manager_v1 *mgr)
{
    (void)mgr;
    MuxToplevels *tl = data;
    tl->manager = NULL;
    notify(tl);
}
static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = manager_toplevel,
    .finished = manager_finished,
};

MuxToplevels *mux_toplevels_new(void (*on_change)(void *data), void *data)
{
    MuxToplevels *tl = g_new0(MuxToplevels, 1);
    tl->tops = g_ptr_array_new_with_free_func((GDestroyNotify)top_free);
    tl->on_change = on_change;
    tl->data = data;
    return tl;
}

void mux_toplevels_bind(MuxToplevels *tl, struct wl_registry *registry,
                        uint32_t name, uint32_t version)
{
    uint32_t v = version < 1 ? version : 1;
    tl->manager = wl_registry_bind(
        registry, name, &zwlr_foreign_toplevel_manager_v1_interface, v);
    zwlr_foreign_toplevel_manager_v1_add_listener(tl->manager,
                                                  &manager_listener, tl);
}

const char *mux_toplevels_active_app_id(MuxToplevels *tl)
{
    for (guint i = 0; i < tl->tops->len; i++) {
        Top *t = g_ptr_array_index(tl->tops, i);
        if (t->activated && t->app_id)
            return t->app_id;
    }
    return NULL;
}

void mux_toplevels_free(MuxToplevels *tl)
{
    if (!tl)
        return;
    g_ptr_array_free(tl->tops, TRUE);
    if (tl->manager)
        zwlr_foreign_toplevel_manager_v1_stop(tl->manager);
    g_free(tl);
}
