#define _GNU_SOURCE
#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MuxConfig {
    GHashTable *values; /* "section.key" -> char* */
};

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\n' || end[-1] == '\r'))
        *--end = '\0';
    return s;
}

MuxConfig *mux_config_load(const char *name)
{
    MuxConfig *cfg = g_new0(MuxConfig, 1);
    cfg->values = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    const char *base = g_get_user_config_dir(); /* XDG_CONFIG_HOME or ~/.config */
    char *path = g_strdup_printf("%s/muxlike/%s.conf", base, name);
    FILE *f = fopen(path, "r");
    if (!f) {
        g_free(path);
        return cfg; /* defaults everywhere — fine */
    }

    char section[64] = "";
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, f) > 0) {
        char *s = trim(line);
        if (*s == '\0' || *s == '#' || *s == ';')
            continue;
        if (*s == '[') {
            char *close = strchr(s, ']');
            if (close) {
                *close = '\0';
                g_strlcpy(section, trim(s + 1), sizeof(section));
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *k = trim(s);
        char *v = trim(eq + 1);
        if (*k == '\0')
            continue;
        char *full = g_strdup_printf("%s.%s", section, k);
        g_hash_table_insert(cfg->values, full, g_strdup(v));
    }
    free(line);
    fclose(f);
    g_free(path);
    return cfg;
}

void mux_config_free(MuxConfig *cfg)
{
    if (!cfg)
        return;
    g_hash_table_destroy(cfg->values);
    g_free(cfg);
}

const char *mux_config_get(MuxConfig *cfg, const char *section,
                           const char *key, const char *def)
{
    char *full = g_strdup_printf("%s.%s", section, key);
    const char *v = g_hash_table_lookup(cfg->values, full);
    g_free(full);
    return v ? v : def;
}

int mux_config_get_int(MuxConfig *cfg, const char *section,
                       const char *key, int def)
{
    const char *v = mux_config_get(cfg, section, key, NULL);
    if (!v)
        return def;
    char *end = NULL;
    long n = strtol(v, &end, 10);
    return (end && *end == '\0') ? (int)n : def;
}

double mux_config_get_double(MuxConfig *cfg, const char *section,
                             const char *key, double def)
{
    const char *v = mux_config_get(cfg, section, key, NULL);
    if (!v)
        return def;
    char *end = NULL;
    double n = strtod(v, &end);
    return (end && *end == '\0') ? n : def;
}

bool mux_config_get_bool(MuxConfig *cfg, const char *section,
                         const char *key, bool def)
{
    const char *v = mux_config_get(cfg, section, key, NULL);
    if (!v)
        return def;
    return g_ascii_strcasecmp(v, "true") == 0 ||
           g_ascii_strcasecmp(v, "yes") == 0 || strcmp(v, "1") == 0;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool mux_color_parse(const char *hex, MuxColor *out)
{
    if (!hex || hex[0] != '#')
        return false;
    const char *h = hex + 1;
    size_t len = strlen(h);
    int v[8];
    for (size_t i = 0; i < len; i++) {
        v[i] = hexval(h[i]);
        if (v[i] < 0)
            return false;
    }
    MuxColor c = { 0, 0, 0, 1 };
    if (len == 3) {
        c.r = (v[0] * 17) / 255.0;
        c.g = (v[1] * 17) / 255.0;
        c.b = (v[2] * 17) / 255.0;
    } else if (len == 6 || len == 8) {
        c.r = (v[0] * 16 + v[1]) / 255.0;
        c.g = (v[2] * 16 + v[3]) / 255.0;
        c.b = (v[4] * 16 + v[5]) / 255.0;
        if (len == 8)
            c.a = (v[6] * 16 + v[7]) / 255.0;
    } else {
        return false;
    }
    *out = c;
    return true;
}

bool mux_config_get_color(MuxConfig *cfg, const char *section,
                          const char *key, const char *def_hex,
                          MuxColor *out)
{
    const char *v = mux_config_get(cfg, section, key, def_hex);
    return v && mux_color_parse(v, out);
}
