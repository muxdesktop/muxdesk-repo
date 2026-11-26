#define _GNU_SOURCE
#include "draw.h"

#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

MuxBuffer *mux_buffer_create(struct wl_shm *shm, int width, int height)
{
    if (width <= 0 || height <= 0)
        return NULL;
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    size_t size = (size_t)stride * (size_t)height;

    int fd = memfd_create("mux-buffer", MFD_CLOEXEC);
    if (fd < 0)
        return NULL;
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return NULL;
    }
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    if (!buffer) {
        munmap(data, size);
        return NULL;
    }

    MuxBuffer *buf = calloc(1, sizeof(*buf));
    buf->buffer = buffer;
    buf->data = data;
    buf->size = size;
    buf->width = width;
    buf->height = height;
    buf->surface = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, width, height, stride);
    buf->cr = cairo_create(buf->surface);
    return buf;
}

void mux_buffer_destroy(MuxBuffer *buf)
{
    if (!buf)
        return;
    cairo_destroy(buf->cr);
    cairo_surface_destroy(buf->surface);
    wl_buffer_destroy(buf->buffer);
    munmap(buf->data, buf->size);
    free(buf);
}

void mux_set_color(cairo_t *cr, const MuxColor *c)
{
    cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
}

void mux_rounded_rect(cairo_t *cr, double x, double y, double w, double h,
                      double r)
{
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI_2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI_2);
    cairo_arc(cr, x + r, y + h - r, r, G_PI_2, G_PI);
    cairo_arc(cr, x + r, y + r, r, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);
}

void mux_fill_rounded_rect(cairo_t *cr, double x, double y, double w,
                           double h, double r, const MuxColor *c)
{
    mux_rounded_rect(cr, x, y, w, h, r);
    mux_set_color(cr, c);
    cairo_fill(cr);
}

static PangoLayout *make_layout(cairo_t *cr, const char *font,
                                const char *text, int bold)
{
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc = pango_font_description_from_string(font);
    if (bold)
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout, text, -1);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    return layout;
}

double mux_text_width(cairo_t *cr, const char *font, const char *text,
                      int bold)
{
    PangoLayout *layout = make_layout(cr, font, text, bold);
    int w = 0;
    pango_layout_get_pixel_size(layout, &w, NULL);
    g_object_unref(layout);
    return w;
}

void mux_text_size(cairo_t *cr, const char *font, const char *text,
                   int bold, double *w_out, double *h_out)
{
    PangoLayout *layout = make_layout(cr, font, text, bold);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    if (w_out) *w_out = w;
    if (h_out) *h_out = h;
    g_object_unref(layout);
}

double mux_draw_text(cairo_t *cr, const char *font, const MuxColor *color,
                     double x, double y, const char *text, int align,
                     int bold)
{
    PangoLayout *layout = make_layout(cr, font, text, bold);
    int w = 0, h = 0;
    pango_layout_get_pixel_size(layout, &w, &h);
    double dx = x;
    if (align == 1)
        dx = x - w / 2.0;
    else if (align == 2)
        dx = x - w;
    mux_set_color(cr, color);
    cairo_move_to(cr, dx, y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    return w;
}

void mux_draw_logo(cairo_t *cr, double x, double y, double size)
{
    MuxColor dark = { 0x12 / 255.0, 0x14 / 255.0, 0x1f / 255.0, 1 };
    double r = size * 0.215; /* ~110/512 like the SVG */
    mux_fill_rounded_rect(cr, x, y, size, size, r, &dark);

    cairo_pattern_t *grad = cairo_pattern_create_linear(x, y, x + size, y + size);
    cairo_pattern_add_color_stop_rgb(grad, 0, 0x7c / 255.0, 0x5c / 255.0, 1.0);
    cairo_pattern_add_color_stop_rgb(grad, 1, 0, 0xc2 / 255.0, 0xa8 / 255.0);

    /* rim */
    mux_rounded_rect(cr, x + size * 0.03, y + size * 0.03,
                     size * 0.94, size * 0.94, r * 0.9);
    cairo_set_source(cr, grad);
    cairo_set_line_width(cr, size * 0.02);
    cairo_stroke(cr);

    /* M stroke: (0.23,0.73) -> (0.23,0.27) -> (0.5,0.52) -> (0.77,0.27) -> (0.77,0.73) */
    cairo_new_path(cr);
    cairo_move_to(cr, x + size * 0.23, y + size * 0.73);
    cairo_line_to(cr, x + size * 0.23, y + size * 0.27);
    cairo_line_to(cr, x + size * 0.50, y + size * 0.52);
    cairo_line_to(cr, x + size * 0.77, y + size * 0.27);
    cairo_line_to(cr, x + size * 0.77, y + size * 0.73);
    cairo_set_source(cr, grad);
    cairo_set_line_width(cr, size * 0.086);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);
    cairo_pattern_destroy(grad);
}
