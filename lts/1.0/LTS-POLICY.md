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
