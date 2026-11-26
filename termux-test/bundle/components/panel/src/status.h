/* mux-panel — status area data sources (battery, network).
 *
 * Honest scope: battery comes from /sys/class/power_supply, network
 * state from /sys/class/net — both readable without root, daemons or
 * D-Bus, which matters in the MuxDesktop proot environment. Signal
 * strength and volume need nl80211 / PipeWire and are NOT faked here;
 * see README "what is still missing". */
#ifndef MUX_STATUS_H
#define MUX_STATUS_H

#include <stdbool.h>

typedef struct {
    bool present;
    int percent;      /* 0..100, -1 when unknown */
    bool charging;
} MuxBattery;

/* Fills *out; present=false when the device has no battery entry. */
void mux_battery_read(MuxBattery *out);

typedef enum {
    MUX_NET_OFFLINE,
    MUX_NET_WIFI,
    MUX_NET_WIRED,
    MUX_NET_OTHER,
} MuxNetState;

/* Label is a static string: "Wi-Fi", "Ethernet", "Offline", ... */
MuxNetState mux_network_state(const char **label_out);

#endif /* MUX_STATUS_H */
