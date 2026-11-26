/* mux-dock — wlr-foreign-toplevel tracking (fuller than mux-panel's
 * copy: the dock lists every window, matches them to dock entries and
 * activates them on click). Own copy per component — see panel's
 * config.h note on the isolation rule. */
#ifndef MUX_TOPLEVEL_H
#define MUX_TOPLEVEL_H

#include <stdbool.h>
#include <wayland-client.h>

typedef struct MuxToplevels MuxToplevels;

MuxToplevels *mux_toplevels_new(void (*on_change)(void *data), void *data);
void mux_toplevels_bind(MuxToplevels *tl, struct wl_registry *registry,
                        uint32_t name, uint32_t version);
int mux_toplevels_count(MuxToplevels *tl);
/* app_id of handle i (may be NULL until the compositor sends it). */
const char *mux_toplevels_app_id(MuxToplevels *tl, int i);
bool mux_toplevels_is_active(MuxToplevels *tl, int i);
/* Activate the first handle whose app_id matches (dock matching rules
 * are the caller's; here it is an exact string compare done by main
 * through mux_app_matches on each handle's app_id). */
void mux_toplevels_activate(MuxToplevels *tl, int i, struct wl_seat *seat);
void mux_toplevels_free(MuxToplevels *tl);

#endif /* MUX_TOPLEVEL_H */
