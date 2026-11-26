/* mux-panel — wl_shm buffer + cairo/pango drawing helpers.
 * Same shape as mux-dock's draw module (own copy per component, see
 * config.h note). Software rendering only: cairo image surfaces on
 * shared memory — no GPU assumption anywhere in MuxLike. */
#ifndef MUX_DRAW_H
#define MUX_DRAW_H

#include "config.h"
#include <cairo.h>
#include <wayland-client.h>

typedef struct {
    struct wl_buffer *buffer;
    cairo_surface_t *surface;
    cairo_t *cr;
    void *data;
    size_t size;
    int width, height;
} MuxBuffer;

/* Create a single ARGB8888 shm buffer. Returns NULL on failure. */
MuxBuffer *mux_buffer_create(struct wl_shm *shm, int width, int height);
void mux_buffer_destroy(MuxBuffer *buf);

void mux_set_color(cairo_t *cr, const MuxColor *c);
void mux_rounded_rect(cairo_t *cr, double x, double y, double w, double h,
                      double r);
void mux_fill_rounded_rect(cairo_t *cr, double x, double y, double w,
                           double h, double r, const MuxColor *c);

/* Text via pango. Returns the layout width in pixels.
 * align: 0 = left (x is left edge), 1 = center (x is center),
 *        2 = right (x is right edge). y is the top edge. */
double mux_draw_text(cairo_t *cr, const char *font, const MuxColor *color,
                     double x, double y, const char *text, int align,
                     int bold);
double mux_text_width(cairo_t *cr, const char *font, const char *text,
                      int bold);
void mux_text_size(cairo_t *cr, const char *font, const char *text,
                   int bold, double *w_out, double *h_out);

/* The MuxDesk mark, drawn as vectors (no image files needed):
 * dark squircle, violet->mint gradient rim, gradient "M" stroke —
 * the same geometry as assets/logo.svg, minus the wordmark. */
void mux_draw_logo(cairo_t *cr, double x, double y, double size);

#endif /* MUX_DRAW_H */
