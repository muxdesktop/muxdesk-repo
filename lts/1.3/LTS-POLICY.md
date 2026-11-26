# MuxOS 1.0 LTS — package freeze policy

MuxOS is an **LTS** distribution, not a rolling release. This document is
the contract for the `lts/1.0` package freeze. The internal repo/plumbing
names stay `muxdesk` (the GitHub repo and pacman repo names); the
user-visible distro name is **MuxOS**.

## What "LTS" means here

1. **Frozen base.** Every package in a MuxOS 1.0 system is pinned to one
   exact version, recorded in `aarch64/manifest.json` (name, version,
   arch, package filename, sha256 where the upstream repo published one,
   sizes, source repo). The manifest is the freeze: 313 pins — the full
   v3.3 aarch64 rootfs set (302 packages) plus the `xorg-xwayland`
   closure (11 packages) added for X11 support.
2. **Frozen binaries for the desktop stack.** The packages that make up
   the desktop/compositor stack and everything installable at first boot
   (224 package files: the v3.3 compositor closure, the xwayland closure,
   and MuxOS's own packages) are hosted as immutable files in this repo
   under `lts/1.0/aarch64/` with a pacman database (`muxdesk-lts.db`).
   Files in an `lts/<version>/` directory are **never replaced or
   updated in place** — a changed freeze is a new directory
   (`lts/1.1/`, `lts/2.0/`, …).
3. **Pin-only base packages.** The remaining 89 base-system packages are
   already baked into the rootfs image. Their exact versions are pinned
   in the manifest and protected by the `IgnorePkg` list shipped with
   the freeze (`aarch64/pins-ignorepkg.txt`), which the rootfs bakes
   into `pacman.conf` — pacman will never upgrade them from the rolling
   upstream mirrors. Their upstream archive URLs are recorded in the
   manifest for provenance.
4. **No rolling drift.** A MuxOS 1.0 install does not follow the Arch
   Linux ARM rolling mirrors for pinned packages. `pacman -Syu` against
   upstream must not change any pinned version; the LTS repo and the
   IgnorePkg pin are what enforce this (this freeze exists because a
   rolling mirror 404 broke first-boot installs on 2026-11-26).
5. **What may change inside 1.0:**
   - MuxOS's **own packages** (mux-*, muxdesk-*, muxlike-*) may be updated
     through the regular `[muxdesk]` repo — that is our software, on our
     schedule.
   - **Security fixes** to frozen upstream packages are *not* silently
     pulled from upstream. A fix is adopted deliberately: the patched
     package is tested, the freeze is re-issued as a new point freeze
     (`lts/1.1/`) with a new manifest, and release notes say exactly
     which pins changed and why.
6. **Support window.** MuxOS 1.0 LTS is the supported aarch64 base until
   a successor LTS freeze is published. Other architectures get their
   own freeze directories when they are built — a freeze is per-arch.

## Using the freeze (rootfs builders)

In `pacman.conf`, put the LTS repo **first** and add the pin list:

```ini
[muxdesk-lts]
SigLevel = Never
Server = https://muxdesktop.github.io/muxdesk-repo/lts/1.0/$arch

[muxdesk]
SigLevel = Never
Server = https://muxdesktop.github.io/muxdesk-repo/$arch
```

plus the `IgnorePkg = …` line from `aarch64/pins-ignorepkg.txt` in
`[options]`. A ready-to-merge snippet is in `aarch64/pacman-lts.conf`.

Freeze identity: base rootfs v3.3 (aarch64, sha256
`0327c3be2943fa1b8032d3b6cb84f2f55175041976af9498ff746186f74a3531`),
frozen 2026-11-26.

## Amendment — freeze 1.1 (2026-11-26)

Freeze **1.1** supersedes 1.0 for new builds. Every 1.0 pin is unchanged;
1.1 only *adds* the runtime libraries the MuxOS own apps are built
against, at the same frozen snapshot moment (versions from the same
sync databases the 1.0 freeze was taken from):

- `libshumate` 1.7.1-1 — Mux Maps
- `webkit2gtk-4.1` 2.54.3-1 — Mux Browser
- `poppler-glib` 26.11.0-1 (+ `poppler`) — Mux PDF Viewer
- `gtksourceview5` 5.22.0-1 — Mux Text Editor
- `gst-plugins-base` / `-good` / `-bad` / `gst-libav` 1.28.9-3 and
  `gst-plugin-gtk4` 0.15.4-2 — Mux Music / Video Player / Camera
  (the `gstreamer` core and `-libs` packages were already in 1.0)

plus their complete dependency closures (167 added packages, including
`ffmpeg`). Freeze 1.1 also fixes a real gap found by the Panel&Dock chat:
`python-gobject` 3.58.1-1 (+ `python-cairo`, `gobject-introspection-runtime`,
`libgirepository`) — PyGObject was missing from the v3.3 rootfs, so no
Python GTK app could run. New totals for 1.1: **480 pins, 391 hosted
package files** (byte total in `aarch64/manifest.json`). Rootfs builders should use the 1.1 repo URL:
`https://muxdesktop.github.io/muxdesk-repo/lts/1.1/$arch`.

## Amendment — freeze 1.2 (2026-11-26)

Freeze **1.2** = freeze 1.1 + one package: **`wf-recorder` 0.6.0-2**
(Mux Recorder's Wayland screen-recording backend, requested by the
Camera & Recorder chat; its library dependencies were already frozen).
The rest of that chat's list — `gstreamer`, `gst-plugin-gtk4`,
`gst-plugins-good`, `ffmpeg` — was already in freezes 1.0/1.1.
New totals: **481 pins, 392 hosted package files**. Rootfs builders
should use `https://muxdesktop.github.io/muxdesk-repo/lts/1.2/$arch`.

## Amendment — freeze 1.3 (2026-11-26)

Freeze **1.3** fixes an internal inconsistency in the v3.3 set, found by
the Launcher chat's aarch64 build: the frozen `gtk4` 4.24.1 requires
glib ≥ 2.89 symbols (`g_timeout_source_new_ns`,
`g_markup_parse_context_get_attribute_position`), but the v3.3 rootfs
pinned `glib2` 2.88.3 — no GTK4 app could link against that pair.

Deliberate correction (the re-freeze mechanism of §5, applied to a
broken pin rather than a security fix): **`glib2` 2.88.3-1 → 2.90.1-1**,
bumped together with the two packages the upstream snapshot pairs with
it — **`pcre2` 10.47-1 → 10.49-1** and **`libsysprof-capture`
50.0-4 → 51.0-1**. The old versions are recorded in the manifest
(`replaces_pin`). Freeze 1.3 also adds **`gtk4-layer-shell` 1.3.0-1**
(mux-launcher runtime dependency). Every other pin is unchanged.
New totals: **482 pins, 393 hosted package files**. Rootfs builders
should use `https://muxdesktop.github.io/muxdesk-repo/lts/1.3/$arch`.
