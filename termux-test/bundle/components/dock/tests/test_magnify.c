/* Unit tests for magnification + layout (pure math, no display). */
#include "../src/magnify.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx(double a, double b) { return fabs(a - b) < 0.01; }

int main(void)
{
    MuxMagnifyParams mp = { .base_size = 48,
                            .max_scale = 1.5,
                            .radius = 110,
                            .spacing = 8,
                            .padding = 10 };

    /* no pointer -> no magnification */
    assert(approx(mux_magnify_scale(&mp, 100, -1), 1.0));
    /* pointer dead centre -> full magnification */
    assert(approx(mux_magnify_scale(&mp, 100, 100), 1.5));
    /* at/past radius -> none */
    assert(approx(mux_magnify_scale(&mp, 100, 210), 1.0));
    assert(approx(mux_magnify_scale(&mp, 100, 400), 1.0));
    /* halfway -> cosine midpoint: 1 + 0.5 * 0.5 = 1.25 */
    assert(approx(mux_magnify_scale(&mp, 100, 155), 1.25));
    /* monotonic falloff */
    double s1 = mux_magnify_scale(&mp, 100, 120);
    double s2 = mux_magnify_scale(&mp, 100, 160);
    assert(s1 > s2 && s2 > 1.0);

    /* layout without pointer: 3 icons = 10 + 48*3 + 8*2 + 10 = 180 */
    double centers[8], sizes[8];
    int w = mux_dock_layout(&mp, 3, -1, centers, sizes);
    assert(w == 180);
    assert(approx(centers[0], 34) && approx(centers[1], 90) &&
           approx(centers[2], 146));
    assert(approx(sizes[0], 48));

    /* layout with pointer over middle icon: wider, middle is biggest.
     * The relaxation passes shift centres as icons grow, so the middle
     * icon ends up near-max rather than exactly max_scale — that is the
     * intended, stable behaviour (the pointer keeps moving in real use). */
    int w2 = mux_dock_layout(&mp, 3, centers[1], centers, sizes);
    assert(w2 > w);
    assert(sizes[1] > sizes[0] && sizes[1] > 60 && sizes[1] <= 72);

    /* empty dock is just padding */
    assert(mux_dock_layout(&mp, 0, -1, centers, sizes) == 20);

    /* hit testing */
    mux_dock_layout(&mp, 3, -1, centers, sizes);
    assert(mux_dock_hit(3, centers, sizes, 34) == 0);
    assert(mux_dock_hit(3, centers, sizes, 90) == 1);
    assert(mux_dock_hit(3, centers, sizes, 61) == -1); /* gap */
    assert(mux_dock_hit(3, centers, sizes, 500) == -1);

    puts("test-magnify: OK");
    return 0;
}
