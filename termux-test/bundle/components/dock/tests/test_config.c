/* Unit tests for the config parser + colour parsing (no display). */
#include "../src/config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Missing file -> defaults, never a crash. */
    MuxConfig *cfg = mux_config_load("definitely-not-a-real-component");
    assert(mux_config_get_int(cfg, "panel", "height", 30) == 30);
    assert(strcmp(mux_config_get(cfg, "panel", "font", "Sans 11"),
                  "Sans 11") == 0);
    assert(mux_config_get_bool(cfg, "panel", "show_battery", true) == true);
    mux_config_free(cfg);

    MuxColor c;
    assert(mux_color_parse("#7c5cff", &c));
    assert(c.r > 0.48 && c.r < 0.49 && c.g > 0.35 && c.g < 0.37 &&
           c.b > 0.99 && c.a == 1.0);
    assert(mux_color_parse("#12141fd9", &c));
    assert(c.a > 0.84 && c.a < 0.86);
    assert(mux_color_parse("#fff", &c));
    assert(c.r == 1.0 && c.g == 1.0 && c.b == 1.0);
    assert(!mux_color_parse("violet", &c));
    assert(!mux_color_parse("#12", &c));
    assert(!mux_color_parse("#gggggg", &c));

    puts("test-config: OK");
    return 0;
}
