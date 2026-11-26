# MuxLike DE — Termux Test Setup

Run the **MuxLike desktop environment** (our own DE: labwc + our C
panel/dock/launcher + Mux Files + Mux Settings) on your phone inside
**Termux**, displayed through **Termux:X11**, using a **Debian proot**.

## Honest note — what this is and is not

This is a **nested test rig**: labwc runs on wlroots' **X11 backend**
inside the Termux:X11 X server. It is **not** the MuxLike app's native
Wayland path. It exists so the DE can be tried on a real phone today.
Expect quirks: software rendering only, some input oddities, and
performance below the native app. Panel/dock/launcher/apps/config are
the real canonical code — only the display path is a stand-in.

## Requirements

- **Termux** (from F-Droid or GitHub — not the outdated Play Store build)
- **Termux:X11 app** installed (separate app; the setup script checks
  and tells you if it is missing)
- ~1.5 GB free space (Debian + packages + build tools), Wi-Fi recommended

## Run it

```sh
# 1) Download and unpack (in Termux)
pkg install -y unzip curl
curl -L -o muxlike-termux-test.zip \
  https://github.com/muxdesktop/muxdesk-repo/releases/download/muxlike-termux-test-v1/muxlike-termux-test.zip
unzip -o muxlike-termux-test.zip

# 2) Setup (long: installs Debian, packages, compiles our components)
bash muxlike-termux-test/muxlike-termux-test.sh

# 3) Start the desktop
bash ~/start-muxlike-test.sh

# 4) Open the Termux:X11 app — the desktop is there.
```

Every step prints `[ OK ]` / `[WARN]` / `[FAIL]` in English. Setup is
safe to re-run; it keeps an existing Debian install.

## What gets installed

- Termux: `proot-distro`, `termux-x11-nightly` (via `x11-repo`)
- Debian (proot, trixie): `labwc` (0.8.3), `swaybg`, `foot`, GTK4 +
  PyGObject, build tools
- Built from the bundled sources in `bundle/` (our C sources are not
  in a public repo yet, so they ship inside this package):
  `mux-panel`, `mux-dock` (make), `mux-launcher` (meson)
- Our labwc config (rc.xml/autostart/theme), MuxLike icons + wallpaper,
  panel/dock/launcher configs, and the Mux Files / Mux Settings apps —
  all into the Debian user's home (`~/.config`, `~/.local`)

## Inside the desktop

- Top bar = our panel (logo menu, active app name, clock, status)
- Bottom dock = our dock (pinned apps, magnification, running dots)
- Super+Space = launcher search, Super+A / F4 = app grid,
  Super+E = Mux Files, Super+Return = terminal (foot)
- Stop: panel logo menu → session exit, or Ctrl+C in Termux

## Troubleshooting

- **Black screen in Termux:X11** — the X server started but labwc did
  not. Re-run the start script and read the Termux output; the usual
  cause is a failed build in setup (re-run the setup script and look
  for `[FAIL]`).
- **Launcher does not open** — its build is the most demanding one
  (GTK4 + gtk4-layer-shell). Check `/tmp/build-launcher.log` inside
  Debian: `proot-distro login debian -- cat /tmp/build-launcher.log`
- **Touch/mouse feels odd** — expected in the nested X11 rig; this is
  the main thing the native app path will do better.
