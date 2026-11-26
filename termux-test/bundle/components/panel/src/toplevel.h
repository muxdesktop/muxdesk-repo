/* mux-panel — wlr-foreign-toplevel tracking.
 *
 * The panel only needs one thing from here: the app_id/title of the
 * currently *activated* toplevel, to show the active app's name next
 * to the Mux logo (macOS menu-bar style). mux-dock has its own fuller
 * copy of this module (per-component isolation, see config.h note). */
#ifndef MUX_TOPLEVEL_H
#define MUX_TOPLEVEL_H

#include <wayland-client.h>

typedef struct MuxToplevels MuxToplevels;

/* on_change fires whenever the activated app may have changed. */
MuxToplevels *mux_toplevels_new(void (*on_change)(void *data), void *data);
/* Call from your registry listener when the manager global appears. */
void mux_toplevels_bind(MuxToplevels *tl, struct wl_registry *registry,
                        uint32_t name, uint32_t version);
/* app_id of the activated toplevel, or NULL. Owned by the module. */
const char *mux_toplevels_active_app_id(MuxToplevels *tl);
void mux_toplevels_free(MuxToplevels *tl);

#endif /* MUX_TOPLEVEL_H */
