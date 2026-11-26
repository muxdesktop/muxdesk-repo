/* mux-dock — MuxLike DE floating dock.
 *
 * Our own code on wlr-layer-shell, same stack as mux-panel
 * (C + cairo on wl_shm, no toolkit, software rendering).
 *   - bottom-centre floating bar, rounded, translucent
 *   - first tile: Mux logo -> opens mux-launcher
 *   - pinned apps from .desktop files + running apps merged in
 *     (wlr-foreign-toplevel), running dot underneath, violet when active
 *   - cosine magnification under the pointer (src/magnify.c, tested)
 *   - click: running -> activate, otherwise launch
 * Icon artwork note: the MuxLike icon theme's real artwork is still
 * being drawn, so tiles are palette-coloured squircles with the app's
 * initial — except when Icon= is a real .png path, which is loaded
 * and drawn as-is. The tile path keeps the dock fully working today.
 */
#define _GNU_SOURCE
#include <glib.h>
#include <math.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>

#include "apps.h"
#include "config.h"
#include "draw.h"
#include "magnify.h"
#include "toplevel.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define MAX_ITEMS 64
#define DOT_AREA 9

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct zwlr_layer_shell_v1 *layer_shell;
    MuxToplevels *toplevels;
    MuxApps *apps;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    int surf_w, surf_h;
    bool configured, running, dirty;

    double pointer_x; /* <0 = pointer not over the dock */
    bool pointer_inside;

    MuxMagnifyParams mp;
    double centers[MAX_ITEMS], sizes[MAX_ITEMS];
    int layout_w;

    MuxConfig *cfg;
    MuxColor bg, fg, muted, accent, mint, coral;
    int margin_bottom, radius;
    bool reserve_space;
    char *launcher_cmd;

    cairo_surface_t *png_cache[MAX_ITEMS]; /* per item index, may be NULL */
    char *png_cache_key[MAX_ITEMS];
} Dock;

/* ---------- helpers ---------- */

static void spawn_cmd(const char *cmd)
{
    if (cmd && *cmd) {
        GError *err = NULL;
        if (!g_spawn_command_line_async(cmd, &err)) {
            g_printerr("mux-dock: cannot run '%s': %s\n", cmd,
                       err ? err->message : "unknown error");
            g_clear_error(&err);
        }
    }
}

static int item_count(Dock *d)
{
    int n = 1 + mux_apps_count(d->apps); /* + launcher tile */
    return n < MAX_ITEMS ? n : MAX_ITEMS;
}

/* Recompute layout; if the width changed, ask the compositor for the
 * new size (the configure event then triggers the redraw). */
static void relayout(Dock *d)
{
    int n = item_count(d);
    int w = mux_dock_layout(&d->mp, n, d->pointer_inside ? d->pointer_x : -1,
                            d->centers, d->sizes);
    if (w != d->layout_w) {
        d->layout_w = w;
        if (d->layer) {
            zwlr_layer_surface_v1_set_size(d->layer, w, d->surf_h);
            wl_surface_commit(d->surface);
        }
    }
    d->dirty = true;
}

static void sync_running(Dock *d)
{
    mux_apps_begin_sync(d->apps);
    int n = mux_toplevels_count(d->toplevels);
    for (int i = 0; i < n; i++) {
        const char *app_id = mux_toplevels_app_id(d->toplevels, i);
        if (app_id)
            mux_apps_note_running(d->apps, app_id,
                                  mux_toplevels_is_active(d->toplevels, i));
    }
    mux_apps_prune(d->apps);
    relayout(d);
}

static void on_toplevel_change(void *data)
{
    sync_running((Dock *)data);
}

/* ---------- icon drawing ---------- */

/* Approved palette mapping (DESIGN §3ক): folder mint, settings/
 * browser/music/terminal violet, store/calendar coral, notes/mail
 * mint, photos multicolour. Matched by desktop id substring. */
static void tile_colors(const Dock *d, const char *id, MuxColor *c1,
                        MuxColor *c2)
{
    *c2 = *c1 = d->accent;
    if (!id) {
        *c2 = d->mint;
        return;
    }
    if (strstr(id, "files") || strstr(id, "notes") || strstr(id, "mail") ||
        strstr(id, "maps") || strstr(id, "camera")) {
        *c1 = *c2 = d->mint;
    } else if (strstr(id, "store") || strstr(id, "calendar") ||
               strstr(id, "weather")) {
        *c1 = *c2 = d->coral;
    } else if (strstr(id, "photos")) {
        *c1 = d->accent;
        *c2 = d->coral;
    }
}

/* Resolve an icon value from a .desktop file to a PNG on disk.
 * Absolute paths pass through; a theme name (e.g. "mux-files") is
 * searched in the MuxLike icon theme's PNG renders under the XDG
 * data dirs. Returns a new string or NULL. */
static char *resolve_icon_path(const char *icon)
{
    if (!icon || !icon[0])
        return NULL;
    if (icon[0] == '/')
        return g_strdup(icon);
    static const char *sizes[] = { "256x256", "128x128", "64x64", "48x48" };
    const char *user_dir = g_get_user_data_dir();
    const gchar * const *sys_dirs = g_get_system_data_dirs();
    int n_sys = 0;
    while (sys_dirs && sys_dirs[n_sys])
        n_sys++;
    char *found = NULL;
    for (int pass = 0; pass < 1 + n_sys && !found; pass++) {
        const char *base = pass == 0 ? user_dir : sys_dirs[pass - 1];
        if (!base)
            break;
        for (unsigned si = 0; si < G_N_ELEMENTS(sizes) && !found; si++) {
            char *cand = g_strdup_printf("%s/icons/MuxLike/%s/apps/%s.png",
                                         base, sizes[si], icon);
            if (g_file_test(cand, G_FILE_TEST_IS_REGULAR))
                found = cand;
            else
                g_free(cand);
        }
    }
    return found;
}

static cairo_surface_t *load_png_icon(Dock *d, int idx, const char *icon)
{
    if (!icon)
        return NULL;
    if (d->png_cache_key[idx] && strcmp(d->png_cache_key[idx], icon) == 0)
        return d->png_cache[idx];
    char *path = resolve_icon_path(icon);
    if (!path)
        return NULL;
    if (d->png_cache[idx])
        cairo_surface_destroy(d->png_cache[idx]);
    g_free(d->png_cache_key[idx]);
    d->png_cache_key[idx] = g_strdup(icon);
    d->png_cache[idx] = cairo_image_surface_create_from_png(path);
    g_free(path);
    if (cairo_surface_status(d->png_cache[idx]) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(d->png_cache[idx]);
        d->png_cache[idx] = NULL;
    }
    return d->png_cache[idx];
}

static void draw_icon(Dock *d, cairo_t *cr, int idx, double cx, double size,
                      double bottom)
{
    double x = cx - size / 2.0;
    double y = bottom - size;
    if (idx == 0) { /* launcher tile: our own logo */
        mux_draw_logo(cr, x, y, size);
        return;
    }
    MuxApp *app = mux_apps_get(d->apps, idx - 1);
    cairo_surface_t *png = load_png_icon(d, idx, app->icon);
    if (png) {
        int pw = cairo_image_surface_get_width(png);
        int ph = cairo_image_surface_get_height(png);
        cairo_save(cr);
        mux_rounded_rect(cr, x, y, size, size, size * 0.22);
        cairo_clip(cr);
        cairo_translate(cr, x, y);
        cairo_scale(cr, size / pw, size / ph);
        cairo_set_source_surface(cr, png, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
        return;
    }
    MuxColor c1, c2;
    tile_colors(d, app->id, &c1, &c2);
    if (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b) {
        mux_fill_rounded_rect(cr, x, y, size, size, size * 0.22, &c1);
    } else {
        cairo_pattern_t *grad = cairo_pattern_create_linear(x, y, x + size,
                                                            y + size);
        cairo_pattern_add_color_stop_rgba(grad, 0, c1.r, c1.g, c1.b, 1);
        cairo_pattern_add_color_stop_rgba(grad, 1, c2.r, c2.g, c2.b, 1);
        mux_rounded_rect(cr, x, y, size, size, size * 0.22);
        cairo_set_source(cr, grad);
        cairo_fill(cr);
        cairo_pattern_destroy(grad);
    }
    /* initial letter */
    char letter[8] = "?";
    gunichar ch = g_utf8_get_char(app->name);
    gunichar up = g_unichar_toupper(ch);
    g_unichar_to_utf8(up, letter);
    char *font = g_strdup_printf("Sans Bold %d", (int)(size * 0.48));
    double tw, th;
    mux_text_size(cr, font, letter, 0, &tw, &th);
    MuxColor white = { 1, 1, 1, 0.95 };
    mux_draw_text(cr, font, &white, cx - tw / 2.0, y + (size - th) / 2.0,
                  letter, 0, 0);
    g_free(font);
}

static void draw_dock(Dock *d)
{
    if (!d->configured || d->surf_w <= 0)
        return;
    MuxBuffer *buf = mux_buffer_create(d->shm, d->surf_w, d->surf_h);
    if (!buf)
        return;
    cairo_t *cr = buf->cr;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    mux_fill_rounded_rect(cr, 0, 0, d->surf_w, d->surf_h, d->radius, &d->bg);
    MuxColor edge = d->fg;
    edge.a = 0.14;
    mux_rounded_rect(cr, 0.5, 0.5, d->surf_w - 1, d->surf_h - 1, d->radius);
    mux_set_color(cr, &edge);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    double bottom = d->surf_h - DOT_AREA;
    int n = item_count(d);
    for (int i = 0; i < n; i++) {
        draw_icon(d, cr, i, d->centers[i], d->sizes[i], bottom);
        if (i > 0) {
            MuxApp *app = mux_apps_get(d->apps, i - 1);
            if (app->running) {
                MuxColor dot = app->active ? d->accent : d->fg;
                if (!app->active)
                    dot.a = 0.65;
                cairo_arc(cr, d->centers[i], d->surf_h - 4.5,
                          app->active ? 2.6 : 2.2, 0, 2 * G_PI);
                mux_set_color(cr, &dot);
                cairo_fill(cr);
            }
        }
    }

    wl_surface_attach(d->surface, buf->buffer, 0, 0);
    wl_surface_damage_buffer(d->surface, 0, 0, d->surf_w, d->surf_h);
    wl_surface_commit(d->surface);
    mux_buffer_destroy(buf);
    d->dirty = false;
}

/* ---------- activation / launch ---------- */

static void item_clicked(Dock *d, int idx)
{
    if (idx == 0) {
        spawn_cmd(d->launcher_cmd);
        return;
    }
    MuxApp *app = mux_apps_get(d->apps, idx - 1);
    if (app->running) {
        int n = mux_toplevels_count(d->toplevels);
        for (int i = 0; i < n; i++) {
            const char *app_id = mux_toplevels_app_id(d->toplevels, i);
            if (app_id && mux_app_matches(app, app_id)) {
                mux_toplevels_activate(d->toplevels, i, d->seat);
                return;
            }
        }
    }
    char *cmd = mux_app_launch_command(app);
    spawn_cmd(cmd);
    g_free(cmd);
}

/* ---------- pointer ---------- */

static void pointer_enter(void *data, struct wl_pointer *ptr, uint32_t serial,
                          struct wl_surface *surface, wl_fixed_t x,
                          wl_fixed_t y)
{
    (void)ptr; (void)serial; (void)surface; (void)y;
    Dock *d = data;
    d->pointer_inside = true;
    d->pointer_x = wl_fixed_to_double(x);
    relayout(d);
}
static void pointer_leave(void *data, struct wl_pointer *ptr, uint32_t serial,
                          struct wl_surface *surface)
{
    (void)ptr; (void)serial; (void)surface;
    Dock *d = data;
    d->pointer_inside = false;
    d->pointer_x = -1;
    relayout(d);
}
static void pointer_motion(void *data, struct wl_pointer *ptr, uint32_t time_,
                           wl_fixed_t x, wl_fixed_t y)
{
    (void)ptr; (void)time_; (void)y;
    Dock *d = data;
    d->pointer_x = wl_fixed_to_double(x);
    relayout(d);
}
static void pointer_button(void *data, struct wl_pointer *ptr, uint32_t serial,
                           uint32_t time_, uint32_t button, uint32_t state)
{
    (void)ptr; (void)serial; (void)time_;
    Dock *d = data;
    if (button != 0x110 || state != WL_POINTER_BUTTON_STATE_PRESSED)
        return;
    int idx = mux_dock_hit(item_count(d), d->centers, d->sizes, d->pointer_x);
    if (idx >= 0)
        item_clicked(d, idx);
}
static void pointer_axis(void *data, struct wl_pointer *ptr, uint32_t time_,
                         uint32_t axis, wl_fixed_t value)
{
    (void)data; (void)ptr; (void)time_; (void)axis; (void)value;
}
static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
};

/* ---------- seat / registry / layer ---------- */

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    Dock *d = data;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
        d->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(d->pointer, &pointer_listener, d);
    }
}
static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data; (void)seat; (void)name;
}
static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void layer_configure(void *data, struct zwlr_layer_surface_v1 *layer,
                            uint32_t serial, uint32_t width, uint32_t height)
{
    Dock *d = data;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
    d->surf_w = (int)width;
    d->surf_h = (int)height;
    d->configured = true;
    d->dirty = true;
}
static void layer_closed(void *data, struct zwlr_layer_surface_v1 *layer)
{
    (void)layer;
    ((Dock *)data)->running = false;
}
static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version)
{
    Dock *d = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        d->compositor = wl_registry_bind(registry, name,
                                         &wl_compositor_interface,
                                         version < 4 ? version : 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        d->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        d->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(d->seat, &seat_listener, d);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        d->layer_shell = wl_registry_bind(
            registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface,
                      zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        mux_toplevels_bind(d->toplevels, registry, name, version);
    }
}
static void registry_remove(void *data, struct wl_registry *registry,
                            uint32_t name)
{
    (void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

/* ---------- config ---------- */

static void load_config(Dock *d)
{
    d->cfg = mux_config_load("dock");
    d->mp.base_size = mux_config_get_int(d->cfg, "dock", "icon_size", 48);
    double mag = mux_config_get_double(d->cfg, "dock", "magnification", 1.35);
    if (!mux_config_get_bool(d->cfg, "dock", "magnify", true))
        mag = 1.0;
    d->mp.max_scale = mag;
    d->mp.radius = mux_config_get_int(d->cfg, "dock", "magnify_radius", 110);
    d->mp.spacing = mux_config_get_int(d->cfg, "dock", "spacing", 8);
    d->mp.padding = mux_config_get_int(d->cfg, "dock", "padding", 10);
    d->margin_bottom = mux_config_get_int(d->cfg, "dock", "margin_bottom", 10);
    d->radius = mux_config_get_int(d->cfg, "dock", "corner_radius", 18);
    d->reserve_space = mux_config_get_bool(d->cfg, "dock", "reserve_space",
                                           false);
    mux_config_get_color(d->cfg, "dock", "background", "#12141fb3", &d->bg);
    mux_config_get_color(d->cfg, "dock", "foreground", "#e8eaf6", &d->fg);
    mux_config_get_color(d->cfg, "dock", "muted", "#a7abc0", &d->muted);
    mux_config_get_color(d->cfg, "dock", "accent", "#7c5cff", &d->accent);
    mux_color_parse("#00c2a8", &d->mint);
    mux_color_parse("#ff6f61", &d->coral);
    d->launcher_cmd = g_strdup(mux_config_get(d->cfg, "dock", "launcher_cmd",
                                              "mux-launcher"));

    /* pinned apps: search the standard applications dirs */
    char *user_apps = g_strdup_printf("%s/applications", g_get_user_data_dir());
    const char *const *sys = g_get_system_data_dirs();
    GPtrArray *dirs = g_ptr_array_new();
    g_ptr_array_add(dirs, user_apps);
    for (int i = 0; sys[i]; i++) {
        char *p = g_strdup_printf("%s/applications", sys[i]);
        g_ptr_array_add(dirs, p);
    }
    g_ptr_array_add(dirs, NULL);
    const char *pinned = mux_config_get(
        d->cfg, "dock", "pinned",
        "mux-files,mux-browser,mux-terminal,mux-settings,mux-store,"
        "mux-music,mux-photos,mux-notes,mux-calendar,mux-mail");
    char **ids = g_strsplit(pinned, ",", -1);
    for (int i = 0; ids[i]; i++) {
        char *id = g_strstrip(ids[i]);
        if (*id)
            mux_apps_add_pinned(d->apps, (char **)dirs->pdata, id);
    }
    g_strfreev(ids);
    g_ptr_array_free(dirs, TRUE); /* strings leak-free enough at startup;
                                      g_ptr_array owns only the array */
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    Dock *d = g_new0(Dock, 1);
    d->running = true;
    d->pointer_x = -1;
    d->apps = mux_apps_new();
    load_config(d);
    d->surf_h = (int)(d->mp.base_size * d->mp.max_scale + 0.5) +
                d->mp.padding * 2 + DOT_AREA;
    d->toplevels = mux_toplevels_new(on_toplevel_change, d);

    d->display = wl_display_connect(NULL);
    if (!d->display) {
        g_printerr("mux-dock: cannot connect to Wayland display "
                   "(WAYLAND_DISPLAY unset? labwc running?)\n");
        return 1;
    }
    d->registry = wl_display_get_registry(d->display);
    wl_registry_add_listener(d->registry, &registry_listener, d);
    wl_display_roundtrip(d->display);
    if (!d->compositor || !d->shm || !d->layer_shell) {
        g_printerr("mux-dock: missing globals — needs a wlroots "
                   "compositor (labwc)\n");
        return 1;
    }

    d->layout_w = mux_dock_layout(&d->mp, item_count(d), -1, d->centers,
                                  d->sizes);
    d->surface = wl_compositor_create_surface(d->compositor);
    d->layer = zwlr_layer_shell_v1_get_layer_surface(
        d->layer_shell, d->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "mux-dock");
    zwlr_layer_surface_v1_set_anchor(d->layer,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_size(d->layer, d->layout_w, d->surf_h);
    zwlr_layer_surface_v1_set_margin(d->layer, 0, 0, d->margin_bottom, 0);
    zwlr_layer_surface_v1_set_exclusive_zone(
        d->layer, d->reserve_space ? d->surf_h + d->margin_bottom : 0);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        d->layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_add_listener(d->layer, &layer_listener, d);
    wl_surface_commit(d->surface);

    int fd = wl_display_get_fd(d->display);
    while (d->running) {
        while (wl_display_prepare_read(d->display) != 0)
            wl_display_dispatch_pending(d->display);
        wl_display_flush(d->display);
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 250);
        if (ret > 0 && (pfd.revents & POLLIN))
            wl_display_read_events(d->display);
        else
            wl_display_cancel_read(d->display);
        wl_display_dispatch_pending(d->display);
        if (d->dirty)
            draw_dock(d);
    }

    zwlr_layer_surface_v1_destroy(d->layer);
    wl_surface_destroy(d->surface);
    wl_display_disconnect(d->display);
    return 0;
}
