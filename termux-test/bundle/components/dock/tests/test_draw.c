/* Headless render test: draws the logo + text on a plain cairo image
 * surface (no Wayland) and checks pixels actually landed. */
#include "../src/draw.h"
#include <assert.h>
#include <cairo.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                       240, 40);
    cairo_t *cr = cairo_create(surf);
    MuxColor bg = { 0x12 / 255.0, 0x14 / 255.0, 0x1f / 255.0, 0.85 };
    mux_fill_rounded_rect(cr, 0, 0, 240, 40, 0, &bg);
    mux_draw_logo(cr, 8, 5, 30);
    MuxColor fg = { 0xe8 / 255.0, 0xea / 255.0, 0xf6 / 255.0, 1 };
    double w = mux_draw_text(cr, "Sans 11", &fg, 48, 12, "MuxDesk", 0, 1);
    assert(w > 20); /* text was laid out */
    cairo_surface_flush(surf);

    unsigned char *px = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);
    /* logo centre (23,20) is on the M stroke / dark tile: not blank bg */
    unsigned char *logo_px = px + 20 * stride + 23 * 4;
    assert(logo_px[3] == 0xff); /* fully painted */
    /* a corner of the bar keeps the translucent bg alpha (~0.85*255) */
    unsigned char *corner = px + 1 * stride + 238 * 4;
    assert(corner[3] > 200 && corner[3] < 230);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    puts("test-draw: OK");
    return 0;
}
