# MuxOS Distro — Termux Test Path (proot-distro + Termux:X11)

Test the **real MuxOS distro** (our own ARM64 rootfs — not stock
Debian, not the MuxLike app) on the phone: install the rootfs into
**proot-distro**, start the DE session nested through **Termux:X11**.

## Honest note — what this is and is not

The distro itself is the real image (our branding, our repo, our DE
stack). Only the *display path* is a test rig: inside proot there is
no GPU/seat, so labwc runs **nested** — on its X11 backend inside
Termux:X11 (software rendering). Input/rendering quirks are expected;
the native app path is the real target. A Wayland nesting option
(Termux:Wayland) is included as experimental.

## Requirements

- **Termux** (F-Droid / GitHub build), **Termux:X11 app** installed
- The **MuxDesk ARM64 rootfs tarball** (`muxdesk-rootfs-aarch64-*.tar.*`)
  from the muxdesk-repo Releases (built by the Installer chat)
- ~2–3 GB free space

## Run it

```sh
# 1) Download and unpack this test package (in Termux)
pkg install -y unzip curl
curl -L -o muxdesk-termux-test.zip \
  https://github.com/muxdesktop/muxdesk-repo/releases/download/muxdesk-termux-test-v1/muxdesk-termux-test.zip
unzip -o muxdesk-termux-test.zip

# 2) Setup — installs proot-distro/Termux:X11 support, then installs
#    the MuxDesk image as the proot-distro container "muxdesk".
#    Pass the image path or its download URL, or put the tarball in
#    ~/ or ~/storage/downloads and it is found automatically.
bash muxdesk-test/muxdesk-termux-setup.sh /path/to/muxdesk-rootfs-aarch64-vX.tar.gz

# 3) Start the DE (X11 test path)
bash ~/start-muxdesk.sh

# 4) Open the Termux:X11 app — the MuxOS desktop is there.
```

Every step prints `[ OK ]` / `[WARN]` / `[FAIL]` in English. Setup is
safe to re-run; an existing `muxdesk` container is kept.

Under the hood, setup uses proot-distro 5.x's local-archive install:
`proot-distro install <tarball> --name muxdesk`. The start script
waits until the X socket really answers (no fixed sleeps), binds a
writable `/dev/shm` into the container (wlroots needs it for the
keyboard keymap — without it labwc exits), bind-mounts this package
at `/root/muxdesk-test`, and runs the image's **own** session chain
(`run-desktop` → `/etc/muxdesk-session` → the DE session). The image
itself is never modified.

## Wayland path (experimental)

Install the **Termux:Wayland** app, open it (it creates its compositor
socket), then:

```sh
bash ~/start-muxdesk-wayland.sh
```

This binds the Wayland socket dir into the container and starts the
same session with `WLR_BACKENDS=wayland`. It is documented and
scripted, but **not yet phone-verified** — expect to report back what
the log says. The X11 path above is the primary test path.

## Troubleshooting

- **"Failed to open xcb connection"** — the X server was not ready
  when labwc started. The start script waits for the socket; if it
  still fails, read `~/muxdesk-x11.log`, open Termux:X11 once, close
  other Termux sessions running an X server, retry.
- **labwc exits mentioning shm/keymap** — the `/dev/shm` bind did not
  apply; re-run with the latest start script and report the proot
  warning lines shown in Termux.
- **"No session entry inside this image"** — the image is the bare
  base without the DE stack (run-desktop/labwc). That is the image
  builders' part, not this test path.
- **Container name** is `muxdesk`: `proot-distro list` shows it, and
  `proot-distro login muxdesk` enters it by hand.
