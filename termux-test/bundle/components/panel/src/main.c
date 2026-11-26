/* mux-panel — MuxLike DE top bar.
 *
 * Our own code on wlr-layer-shell (the wlroots/labwc way — no toolkit):
 *   left : Mux logo (click -> Mux menu) + active app name + the app's
 *          global menu titles (src/appmenu.c, D-Bus AppMenu contract)
 *   right: network label, battery, clock
 * Dropdowns (logo menu and per-title app menus) share one layer
 * surface in the overlay layer. Rendering is cairo on wl_shm —
 * pure software, no GPU needed.
 */
#define _GNU_SOURCE
#include <gio/gio.h>
#include <glib.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>

#include "appmenu.h"
#include "config.h"
#include "draw.h"
#include "status.h"
#include "toplevel.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define MENU_WIDTH 248
#define MENU_ITEM_H 28
#define MENU_PAD 6
#define MAX_TITLES 16
#define MAX_DROP_ITEMS 64

/* One dropdown row, used for both the logo menu (command) and an app
 * menu title's dropdown (app_item, borrowed from the appmenu model —
 * the dropdown is closed before the model is ever rebuilt). */
typedef struct {
    const char *label; /* NULL = separator */
    const char *command;         /* logo dropdown: shell command */
    const AppMenuItem *app_item; /* app dropdown: item to activate */
    bool enabled;
    bool has_children;
} DropItem;

enum SurfKind { SURF_PANEL = 1, SURF_MENU = 2 };

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct zwlr_layer_shell_v1 *layer_shell;
    MuxToplevels *toplevels;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    int width, height;
    bool configured, running, dirty;

    struct wl_surface *menu_surface;
    struct zwlr_layer_surface_v1 *menu_layer;
    int menu_height, menu_width, menu_margin_left;
    bool menu_open, menu_configured;
    int menu_hover; /* item index or -1 */

    DropItem drop_items[MAX_DROP_ITEMS];
    int n_drop_items;
    bool drop_is_app;  /* false = logo menu, true = app menu dropdown */
    int drop_title;    /* app title index when drop_is_app, else -1 */
    char *logo_cmds[8]; /* commands for the logo menu, from config */
    int n_logo_cmds;

    enum SurfKind pointer_on;
    double pointer_x, pointer_y;

    MuxConfig *cfg;
    int cfg_height;
    MuxColor bg, fg, muted, accent, menu_bg;
    char *font;
    char *clock_format;
    bool show_battery, show_network;

    /* global menu */
    GDBusConnection *bus;
    AppMenu *appmenu;
    char *last_app_id;
    double title_x0[MAX_TITLES], title_x1[MAX_TITLES];
    int title_idx[MAX_TITLES]; /* drawn slot -> appmenu title index */
    int n_titles_drawn;
    cairo_t *measure_cr;
    cairo_surface_t *measure_surf;

    MuxBattery battery;
    const char *net_label;
    MuxNetState net_state;
    time_t last_status, last_minute;
} Panel;

static void close_menu(Panel *p);
static void appmenu_changed(void *data);

/* ---------- helpers ---------- */

static void mark_dirty(void *data)
{
    ((Panel *)data)->dirty = true;
}

static void spawn_cmd(const char *cmd)
{
    if (cmd && *cmd) {
        GError *err = NULL;
        if (!g_spawn_command_line_async(cmd, &err)) {
            g_printerr("mux-panel: cannot run '%s': %s\n", cmd,
                       err ? err->message : "unknown error");
            g_clear_error(&err);
        }
    }
}

/* "org.gnome.Nautilus" / "mux-files" -> "Nautilus" / "Mux-files" */
static void pretty_app_name(const char *app_id, char *out, size_t cap)
{
    if (!app_id || !*app_id) {
        g_strlcpy(out, "MuxDesk", cap);
        return;
    }
    const char *base = strrchr(app_id, '/');
    base = base ? base + 1 : app_id;
    const char *dot = strrchr(base, '.');
    if (dot && dot != base && strcmp(dot, ".desktop") != 0)
        base = dot + 1;
    g_strlcpy(out, base, cap);
    if (g_str_has_suffix(out, ".desktop"))
        out[strlen(out) - 8] = '\0';
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] -= 'a' - 'A';
}

/* ---------- drawing ---------- */

static void draw_battery_glyph(cairo_t *cr, double x, double y,
                               const Panel *p)
{
    /* outline 22x11 + tip, fill width by percent, mint when charging,
     * coral under 20% */
    MuxColor col = p->fg;
    if (p->battery.charging) {
        col = (MuxColor){ 0, 0xc2 / 255.0, 0xa8 / 255.0, 1 };
    } else if (p->battery.percent >= 0 && p->battery.percent < 20) {
        col = (MuxColor){ 1, 0x6f / 255.0, 0x61 / 255.0, 1 };
    }
    mux_rounded_rect(cr, x, y, 22, 11, 3);
    mux_set_color(cr, &p->muted);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    mux_fill_rounded_rect(cr, x + 23, y + 3.5, 2.5, 4, 1, &p->muted);
    if (p->battery.percent > 0) {
        double w = 18.0 * p->battery.percent / 100.0;
        mux_fill_rounded_rect(cr, x + 2, y + 2, w, 7, 1.5, &col);
    }
}

/* Wi-Fi fan glyph: three arcs + a dot, ~18px wide. Drawn instead of
 * the old "Network" text label (user: icon, not text). Dim when
 * offline; wired/other get the same fan for now. */
static void draw_wifi_glyph(cairo_t *cr, double cx, double cy,
                            const Panel *p)
{
    MuxColor col = p->fg;
    if (p->net_state == MUX_NET_OFFLINE)
        col = p->muted;
    mux_set_color(cr, &col);
    cairo_set_line_width(cr, 1.7);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    static const double radii[] = { 10.5, 7.0, 3.8 };
    for (unsigned i = 0; i < G_N_ELEMENTS(radii); i++) {
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, radii[i], 3.927, 5.498); /* 225°..315° */
        cairo_stroke(cr);
    }
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, 1.4, 0, 6.2832);
    cairo_fill(cr);
}

static void draw_panel(Panel *p)
{
    if (!p->configured || p->width <= 0)
        return;
    MuxBuffer *buf = mux_buffer_create(p->shm, p->width, p->height);
    if (!buf)
        return;
    cairo_t *cr = buf->cr;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    mux_fill_rounded_rect(cr, 0, 0, p->width, p->height, 0, &p->bg);
    /* hairline at the bottom */
    MuxColor line = p->accent;
    line.a = 0.25;
    cairo_set_source_rgba(cr, line.r, line.g, line.b, line.a);
    cairo_rectangle(cr, 0, p->height - 1, p->width, 1);
    cairo_fill(cr);

    double th;
    mux_text_size(cr, p->font, "Ag", 0, NULL, &th);
    double text_y = (p->height - th) / 2.0;

    /* left: logo + active app */
    double logo_size = p->height - 10;
    if (p->menu_open)
        mux_fill_rounded_rect(cr, 4, 3, logo_size + 8, p->height - 6, 6,
                              &p->accent);
    mux_draw_logo(cr, 8, 5, logo_size);
    char app[128];
    pretty_app_name(mux_toplevels_active_app_id(p->toplevels), app,
                    sizeof(app));
    double name_x = 8 + logo_size + 10;
    double name_w = mux_draw_text(cr, p->font, &p->fg, name_x, text_y, app, 0, 1);

    /* global menu titles of the focused app, macOS-style after the
     * (bold) app name; hit-ranges are kept for click handling */
    p->n_titles_drawn = 0;
    if (p->appmenu && mux_appmenu_has_menu(p->appmenu)) {
        double tx = name_x + name_w + 18;
        int n = mux_appmenu_title_count(p->appmenu);
        if (n > MAX_TITLES)
            n = MAX_TITLES;
        for (int i = 0; i < n; i++) {
            const AppMenuTitle *t = mux_appmenu_title(p->appmenu, i);
            if (!t || !t->label || !*t->label)
                continue;
            double tw = mux_text_width(cr, p->font, t->label, 0);
            if (tx + tw > p->width - 220)
                break; /* keep the status area clear */
            if (p->menu_open && p->drop_is_app && p->drop_title == i) {
                MuxColor hl = p->accent;
                hl.a = 0.35;
                mux_fill_rounded_rect(cr, tx - 7, 3, tw + 14, p->height - 6,
                                      6, &hl);
            }
            mux_draw_text(cr, p->font, &p->fg, tx, text_y, t->label, 0, 0);
            p->title_x0[p->n_titles_drawn] = tx - 7;
            p->title_x1[p->n_titles_drawn] = tx + tw + 7;
            p->title_idx[p->n_titles_drawn] = i;
            p->n_titles_drawn++;
            tx += tw + 22;
        }
    }

    /* right: clock, then battery, then network (right to left) */
    char clock_buf[128] = "";
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(clock_buf, sizeof(clock_buf), p->clock_format, &tmv);
    double x = p->width - 10;
    double cw = mux_draw_text(cr, p->font, &p->fg, x, text_y, clock_buf, 2, 0);
    x -= cw + 16;

    if (p->show_battery && p->battery.present) {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", p->battery.percent);
        double pw = mux_draw_text(cr, p->font, &p->fg, x, text_y, pct, 2, 0);
        x -= pw + 4;
        draw_battery_glyph(cr, x - 26, (p->height - 11) / 2.0, p);
        x -= 26 + 14;
    }
    if (p->show_network) {
        draw_wifi_glyph(cr, x - 9, p->height / 2.0 + 4, p);
        x -= 18 + 14;
    }
    (void)x;

    wl_surface_attach(p->surface, buf->buffer, 0, 0);
    wl_surface_damage_buffer(p->surface, 0, 0, p->width, p->height);
    wl_surface_commit(p->surface);
    mux_buffer_destroy(buf);
    p->dirty = false;
}

static int menu_content_height(const Panel *p)
{
    int h = MENU_PAD * 2;
    for (int i = 0; i < p->n_drop_items; i++)
        h += p->drop_items[i].label ? MENU_ITEM_H : 9;
    return h;
}

static void draw_menu(Panel *p)
{
    if (!p->menu_configured)
        return;
    MuxBuffer *buf = mux_buffer_create(p->shm, p->menu_width, p->menu_height);
    if (!buf)
        return;
    cairo_t *cr = buf->cr;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    mux_fill_rounded_rect(cr, 0, 0, p->menu_width, p->menu_height, 10,
                          &p->menu_bg);

    double th;
    mux_text_size(cr, p->font, "Ag", 0, NULL, &th);
    double y = MENU_PAD;
    for (int i = 0; i < p->n_drop_items; i++) {
        const DropItem *it = &p->drop_items[i];
        if (!it->label) {
            MuxColor sep = p->muted;
            sep.a = 0.35;
            cairo_set_source_rgba(cr, sep.r, sep.g, sep.b, sep.a);
            cairo_rectangle(cr, 12, y + 4, p->menu_width - 24, 1);
            cairo_fill(cr);
            y += 9;
            continue;
        }
        if (i == p->menu_hover && it->enabled)
            mux_fill_rounded_rect(cr, 5, y, p->menu_width - 10, MENU_ITEM_H,
                                  6, &p->accent);
        const MuxColor *col = it->enabled ? &p->fg : &p->muted;
        mux_draw_text(cr, p->font, col, 14, y + (MENU_ITEM_H - th) / 2.0,
                      it->label, 0, 0);
        if (it->has_children)
            mux_draw_text(cr, p->font, &p->muted, p->menu_width - 12,
                          y + (MENU_ITEM_H - th) / 2.0, "▸", 2, 0);
        y += MENU_ITEM_H;
    }
    wl_surface_attach(p->menu_surface, buf->buffer, 0, 0);
    wl_surface_damage_buffer(p->menu_surface, 0, 0, p->menu_width,
                             p->menu_height);
    wl_surface_commit(p->menu_surface);
    mux_buffer_destroy(buf);
}

static int menu_item_at(const Panel *p, double y)
{
    double cy = MENU_PAD;
    for (int i = 0; i < p->n_drop_items; i++) {
        int h = p->drop_items[i].label ? MENU_ITEM_H : 9;
        if (y >= cy && y < cy + h)
            return p->drop_items[i].label ? i : -1;
        cy += h;
    }
    return -1;
}

/* ---------- menu open/close ---------- */

static void menu_layer_configure(void *data,
                                 struct zwlr_layer_surface_v1 *layer,
                                 uint32_t serial, uint32_t width,
                                 uint32_t height)
{
    Panel *p = data;
    (void)width;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
    p->menu_height = (int)height;
    p->menu_configured = true;
    draw_menu(p);
}
static void menu_layer_closed(void *data, struct zwlr_layer_surface_v1 *layer)
{
    (void)data; (void)layer;
}
static const struct zwlr_layer_surface_v1_listener menu_layer_listener = {
    .configure = menu_layer_configure,
    .closed = menu_layer_closed,
};

static void close_menu(Panel *p)
{
    if (!p->menu_open)
        return;
    zwlr_layer_surface_v1_destroy(p->menu_layer);
    wl_surface_destroy(p->menu_surface);
    p->menu_layer = NULL;
    p->menu_surface = NULL;
    p->menu_open = false;
    p->menu_configured = false;
    p->menu_hover = -1;
    p->n_drop_items = 0;
    p->drop_is_app = false;
    p->drop_title = -1;
    p->dirty = true;
}

/* Shared dropdown surface for the logo menu and app menu titles. */
static void create_dropdown(Panel *p, int margin_left, int width)
{
    p->menu_width = width;
    p->menu_margin_left = margin_left;
    p->menu_surface = wl_compositor_create_surface(p->compositor);
    wl_surface_set_user_data(p->menu_surface, (void *)(long)SURF_MENU);
    p->menu_layer = zwlr_layer_shell_v1_get_layer_surface(
        p->layer_shell, p->menu_surface, NULL,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "mux-panel-menu");
    zwlr_layer_surface_v1_set_anchor(
        p->menu_layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                           ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    p->menu_height = menu_content_height(p);
    zwlr_layer_surface_v1_set_size(p->menu_layer, width, p->menu_height);
    zwlr_layer_surface_v1_set_margin(p->menu_layer, p->cfg_height + 4, 0, 0,
                                     margin_left);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        p->menu_layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_add_listener(p->menu_layer, &menu_layer_listener, p);
    p->menu_open = true;
    wl_surface_commit(p->menu_surface);
    p->dirty = true;
}

static const struct {
    const char *label; /* NULL = separator */
    const char *cfg_key;
    const char *def_cmd;
} LOGO_MENU[] = {
    { "About MuxDesk", "about_cmd", "muxinfo" },
    { "Mux Settings", "settings_cmd", "mux-settings" },
    { NULL, NULL, NULL },
    { "Sleep", "sleep_cmd", "systemctl suspend" },
    { "Restart…", "restart_cmd", "systemctl reboot" },
    { "Shut Down…", "shutdown_cmd", "systemctl poweroff" },
    { NULL, NULL, NULL },
    { "Log Out…", "logout_cmd", "mux-logout" },
};

static void open_logo_menu(Panel *p)
{
    if (p->menu_open && !p->drop_is_app) {
        close_menu(p);
        return;
    }
    if (p->menu_open)
        close_menu(p);
    p->n_drop_items = 0;
    int cmd = 0;
    for (guint i = 0; i < G_N_ELEMENTS(LOGO_MENU); i++) {
        DropItem *it = &p->drop_items[p->n_drop_items++];
        it->label = LOGO_MENU[i].label;
        it->app_item = NULL;
        it->enabled = true;
        it->has_children = false;
        it->command = LOGO_MENU[i].label ? p->logo_cmds[cmd++] : NULL;
    }
    p->drop_is_app = false;
    p->drop_title = -1;
    create_dropdown(p, 6, MENU_WIDTH);
}

static cairo_t *measure_context(Panel *p)
{
    if (!p->measure_cr) {
        p->measure_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        p->measure_cr = cairo_create(p->measure_surf);
    }
    return p->measure_cr;
}

static void open_app_menu(Panel *p, int title_index)
{
    if (p->menu_open && p->drop_is_app && p->drop_title == title_index) {
        close_menu(p);
        return;
    }
    if (p->menu_open)
        close_menu(p);
    const AppMenuTitle *t = mux_appmenu_title(p->appmenu, title_index);
    if (!t)
        return;
    p->n_drop_items = 0;
    double longest = 120;
    cairo_t *mcr = measure_context(p);
    for (guint i = 0; i < t->items->len && p->n_drop_items < MAX_DROP_ITEMS;
         i++) {
        const AppMenuItem *ai = g_ptr_array_index(t->items, i);
        DropItem *it = &p->drop_items[p->n_drop_items++];
        it->label = ai->label;
        it->command = NULL;
        it->app_item = ai;
        it->enabled = ai->enabled;
        it->has_children = ai->has_children;
        if (ai->label) {
            double w = mux_text_width(mcr, p->font, ai->label, 0);
            if (w > longest)
                longest = w;
        }
    }
    if (p->n_drop_items == 0)
        return;
    p->drop_is_app = true;
    p->drop_title = title_index;
    int width = (int)(longest + 44);
    if (width < 180)
        width = 180;
    if (width > 380)
        width = 380;
    /* align the dropdown under its title (find the drawn slot) */
    int margin = 6;
    for (int s = 0; s < p->n_titles_drawn; s++)
        if (p->title_idx[s] == title_index)
            margin = (int)p->title_x0[s];
    create_dropdown(p, margin, width);
}

static void appmenu_changed(void *data)
{
    Panel *p = data;
    /* the model was rebuilt — borrowed item pointers are gone, so an
     * open app dropdown must close before anything can use them */
    if (p->menu_open && p->drop_is_app)
        close_menu(p);
    p->dirty = true;
}

/* ---------- pointer ---------- */

static void pointer_enter(void *data, struct wl_pointer *ptr,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t x, wl_fixed_t y)
{
    (void)ptr; (void)serial;
    Panel *p = data;
    p->pointer_on = (enum SurfKind)(long)wl_surface_get_user_data(surface);
    p->pointer_x = wl_fixed_to_double(x);
    p->pointer_y = wl_fixed_to_double(y);
    if (p->pointer_on == SURF_MENU) {
        p->menu_hover = menu_item_at(p, p->pointer_y);
        draw_menu(p);
    }
}
static void pointer_leave(void *data, struct wl_pointer *ptr,
                          uint32_t serial, struct wl_surface *surface)
{
    (void)ptr; (void)serial; (void)surface;
    Panel *p = data;
    p->pointer_on = 0;
    if (p->menu_hover != -1) {
        p->menu_hover = -1;
        if (p->menu_open)
            draw_menu(p);
    }
}
static void pointer_motion(void *data, struct wl_pointer *ptr, uint32_t time_,
                           wl_fixed_t x, wl_fixed_t y)
{
    (void)ptr; (void)time_;
    Panel *p = data;
    p->pointer_x = wl_fixed_to_double(x);
    p->pointer_y = wl_fixed_to_double(y);
    if (p->pointer_on == SURF_MENU) {
        int item = menu_item_at(p, p->pointer_y);
        if (item != p->menu_hover) {
            p->menu_hover = item;
            draw_menu(p);
        }
    }
}
static void pointer_button(void *data, struct wl_pointer *ptr,
                           uint32_t serial, uint32_t time_, uint32_t button,
                           uint32_t state)
{
    (void)ptr; (void)serial; (void)time_;
    Panel *p = data;
    if (button != 0x110 || state != WL_POINTER_BUTTON_STATE_PRESSED)
        return; /* BTN_LEFT press only */
    if (p->pointer_on == SURF_PANEL) {
        double logo_right = 8 + (p->height - 10) + 8;
        if (p->pointer_x <= logo_right) {
            open_logo_menu(p); /* toggles */
            return;
        }
        for (int s = 0; s < p->n_titles_drawn; s++) {
            if (p->pointer_x >= p->title_x0[s] &&
                p->pointer_x <= p->title_x1[s]) {
                open_app_menu(p, p->title_idx[s]); /* toggles */
                return;
            }
        }
        if (p->menu_open)
            close_menu(p);
    } else if (p->pointer_on == SURF_MENU) {
        int item = menu_item_at(p, p->pointer_y);
        if (item < 0)
            return;
        const DropItem *it = &p->drop_items[item];
        if (p->drop_is_app) {
            if (it->enabled && it->app_item && !it->has_children) {
                const AppMenuItem *ai = it->app_item;
                close_menu(p);
                mux_appmenu_activate(p->appmenu, ai);
            }
        } else if (it->command) {
            const char *cmd = it->command;
            close_menu(p);
            spawn_cmd(cmd);
        }
    }
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

/* ---------- seat / registry ---------- */

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t caps)
{
    Panel *p = data;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !p->pointer) {
        p->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(p->pointer, &pointer_listener, p);
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
    Panel *p = data;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
    p->width = (int)width;
    p->height = (int)height;
    p->configured = true;
    p->dirty = true;
}
static void layer_closed(void *data, struct zwlr_layer_surface_v1 *layer)
{
    (void)layer;
    ((Panel *)data)->running = false;
}
static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version)
{
    Panel *p = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        p->compositor = wl_registry_bind(registry, name,
                                         &wl_compositor_interface,
                                         version < 4 ? version : 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        p->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        p->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(p->seat, &seat_listener, p);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        p->layer_shell = wl_registry_bind(
            registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface,
                      zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        mux_toplevels_bind(p->toplevels, registry, name, version);
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

/* ---------- config / menu items ---------- */

static void load_config(Panel *p)
{
    p->cfg = mux_config_load("panel");
    p->cfg_height = mux_config_get_int(p->cfg, "panel", "height", 30);
    mux_config_get_color(p->cfg, "panel", "background", "#12141fd9", &p->bg);
    mux_config_get_color(p->cfg, "panel", "foreground", "#e8eaf6", &p->fg);
    mux_config_get_color(p->cfg, "panel", "muted", "#a7abc0", &p->muted);
    mux_config_get_color(p->cfg, "panel", "accent", "#7c5cff", &p->accent);
    mux_config_get_color(p->cfg, "menu", "background", "#1b1e2af2",
                         &p->menu_bg);
    p->font = g_strdup(mux_config_get(p->cfg, "panel", "font", "Sans 11"));
    p->clock_format = g_strdup(mux_config_get(p->cfg, "panel", "clock_format",
                                              "%a %d %b  %I:%M %p"));
    p->show_battery = mux_config_get_bool(p->cfg, "panel", "show_battery",
                                          true);
    p->show_network = mux_config_get_bool(p->cfg, "panel", "show_network",
                                          true);

    /* logo menu commands, aligned with the non-separator LOGO_MENU rows */
    p->n_logo_cmds = 0;
    for (guint i = 0; i < G_N_ELEMENTS(LOGO_MENU); i++) {
        if (!LOGO_MENU[i].label)
            continue;
        p->logo_cmds[p->n_logo_cmds++] = g_strdup(mux_config_get(
            p->cfg, "menu", LOGO_MENU[i].cfg_key, LOGO_MENU[i].def_cmd));
    }
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    Panel *p = g_new0(Panel, 1);
    p->running = true;
    p->menu_hover = -1;
    load_config(p);
    p->height = p->cfg_height;

    p->toplevels = mux_toplevels_new(mark_dirty, p);
    mux_battery_read(&p->battery);
    p->net_state = mux_network_state(&p->net_label);
    p->last_status = time(NULL);
    p->drop_title = -1;

    /* Global menu: session bus + AppMenu registrar. Entirely optional —
     * without a bus the panel is exactly the old panel. */
    {
        GError *err = NULL;
        p->bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
        if (!p->bus) {
            g_printerr("mux-panel: no session bus (%s) — global menu off\n",
                       err ? err->message : "unknown");
            g_clear_error(&err);
        }
    }
    p->appmenu = mux_appmenu_new(p->bus, appmenu_changed, p);
    if (p->bus)
        mux_appmenu_start_registrar(p->appmenu);

    p->display = wl_display_connect(NULL);
    if (!p->display) {
        g_printerr("mux-panel: cannot connect to Wayland display "
                   "(WAYLAND_DISPLAY unset? labwc running?)\n");
        return 1;
    }
    p->registry = wl_display_get_registry(p->display);
    wl_registry_add_listener(p->registry, &registry_listener, p);
    wl_display_roundtrip(p->display);
    if (!p->compositor || !p->shm || !p->layer_shell) {
        g_printerr("mux-panel: missing globals (compositor=%p shm=%p "
                   "layer-shell=%p) — needs a wlroots compositor (labwc)\n",
                   (void *)p->compositor, (void *)p->shm,
                   (void *)p->layer_shell);
        return 1;
    }

    p->surface = wl_compositor_create_surface(p->compositor);
    wl_surface_set_user_data(p->surface, (void *)(long)SURF_PANEL);
    p->layer = zwlr_layer_shell_v1_get_layer_surface(
        p->layer_shell, p->surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "mux-panel");
    zwlr_layer_surface_v1_set_anchor(p->layer,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(p->layer, 0, p->cfg_height);
    zwlr_layer_surface_v1_set_exclusive_zone(p->layer, p->cfg_height);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        p->layer, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_add_listener(p->layer, &layer_listener, p);
    wl_surface_commit(p->surface);

    int fd = wl_display_get_fd(p->display);
    GMainContext *glib_ctx = g_main_context_default();
    while (p->running) {
        while (wl_display_prepare_read(p->display) != 0)
            wl_display_dispatch_pending(p->display);
        wl_display_flush(p->display);
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 50);
        if (ret > 0 && (pfd.revents & POLLIN))
            wl_display_read_events(p->display);
        else
            wl_display_cancel_read(p->display);
        wl_display_dispatch_pending(p->display);

        /* D-Bus (global menu) lives on the glib main context */
        while (g_main_context_pending(glib_ctx))
            g_main_context_dispatch(glib_ctx);

        /* focus change -> re-resolve the global menu */
        const char *aid = mux_toplevels_active_app_id(p->toplevels);
        if (g_strcmp0(aid, p->last_app_id) != 0) {
            g_free(p->last_app_id);
            p->last_app_id = g_strdup(aid);
            if (p->menu_open && p->drop_is_app)
                close_menu(p);
            mux_appmenu_set_focus(p->appmenu, aid);
            p->dirty = true;
        }

        time_t now = time(NULL);
        if (now != p->last_minute) { /* clock tick (also covers seconds) */
            p->last_minute = now;
            p->dirty = true;
        }
        if (now - p->last_status >= 10) {
            p->last_status = now;
            mux_battery_read(&p->battery);
            p->net_state = mux_network_state(&p->net_label);
            p->dirty = true;
        }
        if (p->dirty)
            draw_panel(p);
    }

    if (p->menu_open)
        close_menu(p);
    zwlr_layer_surface_v1_destroy(p->layer);
    wl_surface_destroy(p->surface);
    wl_display_disconnect(p->display);
    return 0;
}
