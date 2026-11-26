#define _GNU_SOURCE
#include "status.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_file(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' '))
        buf[--n] = '\0';
    return true;
}

void mux_battery_read(MuxBattery *out)
{
    out->present = false;
    out->percent = -1;
    out->charging = false;

    DIR *dir = opendir("/sys/class/power_supply");
    if (!dir)
        return;
    struct dirent *e;
    char path[512], buf[128];
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type",
                 e->d_name);
        if (!read_file(path, buf, sizeof(buf)) || strcmp(buf, "Battery") != 0)
            continue;
        out->present = true;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity",
                 e->d_name);
        if (read_file(path, buf, sizeof(buf)))
            out->percent = atoi(buf);
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status",
                 e->d_name);
        if (read_file(path, buf, sizeof(buf)))
            out->charging = strcmp(buf, "Charging") == 0 ||
                            strcmp(buf, "Full") == 0;
        break; /* first battery is enough for a phone */
    }
    closedir(dir);
}

MuxNetState mux_network_state(const char **label_out)
{
    static const char *labels[] = { "Offline", "Wi-Fi", "Ethernet", "Network" };
    MuxNetState best = MUX_NET_OFFLINE;

    DIR *dir = opendir("/sys/class/net");
    if (dir) {
        struct dirent *e;
        char path[512], buf[64];
        while ((e = readdir(dir)) != NULL) {
            if (e->d_name[0] == '.' || strcmp(e->d_name, "lo") == 0)
                continue;
            snprintf(path, sizeof(path), "/sys/class/net/%s/operstate",
                     e->d_name);
            if (!read_file(path, buf, sizeof(buf)) || strcmp(buf, "up") != 0)
                continue;
            MuxNetState s = MUX_NET_OTHER;
            if (strncmp(e->d_name, "wl", 2) == 0)
                s = MUX_NET_WIFI;
            else if (strncmp(e->d_name, "et", 2) == 0 ||
                     strncmp(e->d_name, "en", 2) == 0)
                s = MUX_NET_WIRED;
            if (s > best)
                best = s;
            if (best == MUX_NET_WIFI)
                break;
        }
        closedir(dir);
    }
    if (label_out)
        *label_out = labels[best];
    return best;
}
