/* mux-dock — tiny INI config reader.
 *
 * Deliberately shared shape with mux-dock's config module: each component
 * keeps its own copy because components must build and ship independently
 * (folder-isolation rule for the MuxLike DE work split). Keep the two
 * copies in sync when the parser itself changes.
 *
 * Format:  [section] / key = value / # or ; comments.
 * Lookup keys are "section.key". Everything has a compiled-in default, so
 * a missing config file is never fatal.
 */
#ifndef MUX_CONFIG_H
#define MUX_CONFIG_H

#include <stdbool.h>

typedef struct {
    double r, g, b, a; /* 0..1 */
} MuxColor;

typedef struct MuxConfig MuxConfig;

/* Load $XDG_CONFIG_HOME/muxlike/<name>.conf (or ~/.config/...).
 * Returns an empty config (all defaults) when no file exists. */
MuxConfig *mux_config_load(const char *name);
void mux_config_free(MuxConfig *cfg);

const char *mux_config_get(MuxConfig *cfg, const char *section,
                           const char *key, const char *def);
int mux_config_get_int(MuxConfig *cfg, const char *section,
                       const char *key, int def);
double mux_config_get_double(MuxConfig *cfg, const char *section,
                             const char *key, double def);
bool mux_config_get_bool(MuxConfig *cfg, const char *section,
                         const char *key, bool def);
/* Accepts #rgb, #rrggbb, #rrggbbaa. Returns false (and leaves *out
 * untouched) on a malformed value so the caller keeps its default. */
bool mux_config_get_color(MuxConfig *cfg, const char *section,
                          const char *key, const char *def_hex,
                          MuxColor *out);
bool mux_color_parse(const char *hex, MuxColor *out);

#endif /* MUX_CONFIG_H */
