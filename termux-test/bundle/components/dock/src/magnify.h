/* mux-dock — magnification + layout math.
 * Pure functions, no Wayland/cairo: this is the part that makes the
 * dock feel like a dock, and it is fully unit-tested (tests/). */
#ifndef MUX_MAGNIFY_H
#define MUX_MAGNIFY_H

typedef struct {
    double base_size;  /* unmagnified icon size, px */
    double max_scale;  /* scale directly under the pointer, e.g. 1.35 */
    double radius;     /* px around the pointer where magnification applies */
    int spacing;       /* px between icons (at base scale) */
    int padding;       /* px at both ends of the dock */
} MuxMagnifyParams;

/* Scale for an icon centred at center_x with the pointer at pointer_x.
 * pointer_x < 0 means "no pointer" -> always 1. Cosine falloff:
 * max_scale at d=0, smoothly back to 1 at d >= radius. */
double mux_magnify_scale(const MuxMagnifyParams *mp, double center_x,
                         double pointer_x);

/* Lay out n icons. Fills centers_out[i] / sizes_out[i] (arrays of n,
 * may be NULL to only measure) and returns the total dock width.
 * Two relaxation passes: scales depend on centres, centres on scales. */
int mux_dock_layout(const MuxMagnifyParams *mp, int n, double pointer_x,
                    double *centers_out, double *sizes_out);

/* Index of the icon containing x, or -1. centres/sizes from layout. */
int mux_dock_hit(int n, const double *centers, const double *sizes,
                 double x);

#endif /* MUX_MAGNIFY_H */
