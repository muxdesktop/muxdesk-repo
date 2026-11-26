#define _GNU_SOURCE /* M_PI under -std=c11 */
#include "magnify.h"

#include <math.h>
#include <stddef.h>

double mux_magnify_scale(const MuxMagnifyParams *mp, double center_x,
                         double pointer_x)
{
    if (pointer_x < 0 || mp->max_scale <= 1.0 || mp->radius <= 0)
        return 1.0;
    double d = fabs(center_x - pointer_x);
    if (d >= mp->radius)
        return 1.0;
    double fall = 0.5 + 0.5 * cos(M_PI * d / mp->radius);
    return 1.0 + (mp->max_scale - 1.0) * fall;
}

int mux_dock_layout(const MuxMagnifyParams *mp, int n, double pointer_x,
                    double *centers_out, double *sizes_out)
{
    if (n <= 0)
        return mp->padding * 2;

    double sizes[64];
    double centers[64];
    if (n > 64)
        n = 64;

    /* pass 0: unmagnified centres */
    double x = mp->padding;
    for (int i = 0; i < n; i++) {
        sizes[i] = mp->base_size;
        centers[i] = x + mp->base_size / 2.0;
        x += mp->base_size + mp->spacing;
    }
    /* two relaxation passes are plenty for a smooth, stable result */
    for (int pass = 0; pass < 2; pass++) {
        x = mp->padding;
        for (int i = 0; i < n; i++) {
            sizes[i] = mp->base_size *
                       mux_magnify_scale(mp, centers[i], pointer_x);
            centers[i] = x + sizes[i] / 2.0;
            x += sizes[i] + mp->spacing;
        }
    }

    if (centers_out && sizes_out) {
        for (int i = 0; i < n; i++) {
            centers_out[i] = centers[i];
            sizes_out[i] = sizes[i];
        }
    }
    return (int)(x - mp->spacing + mp->padding + 0.5);
}

int mux_dock_hit(int n, const double *centers, const double *sizes, double x)
{
    for (int i = 0; i < n; i++) {
        if (x >= centers[i] - sizes[i] / 2.0 &&
            x <= centers[i] + sizes[i] / 2.0)
            return i;
    }
    return -1;
}
