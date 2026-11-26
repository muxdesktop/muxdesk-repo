#!/data/data/com.termux/files/usr/bin/bash
# MuxOS distro — Termux test setup (run in Termux)
#
# Installs OUR OWN MuxDesk ARM64 rootfs into proot-distro as the
# container "muxdesk", so the DE can be tested through Termux:X11
# (and later Termux:Wayland). This is NOT stock Debian and NOT the
# MuxLike app — it is the real MuxOS distro image, run nested.
#
# Usage:
#   bash muxdesk-termux-setup.sh [IMAGE]
# IMAGE may be:
#   - a path to the rootfs tarball (muxdesk-rootfs-aarch64-*.tar.*)
#   - an https:// download URL for it
#   - omitted: the script searches ~, ~/storage/downloads and the
#     current folder for muxdesk-rootfs-aarch64-*.tar.*
#
# Safe to re-run: an existing "muxdesk" container is kept (resume).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
CONTAINER="muxdesk"
step() { echo ""; echo "==> $*"; }
ok()   { echo "[ OK ] $*"; }
warn() { echo "[WARN] $*"; }
fail() { echo "[FAIL] $*"; }
die()  { fail "$*"; echo "Setup stopped. Fix the problem above and run this script again — it is safe to re-run."; exit 1; }

step "MuxOS distro — Termux test setup"
[ -n "${PREFIX:-}" ] && [ -d "$PREFIX" ] || die "This script must run inside Termux (PREFIX is not set)."
command -v pkg >/dev/null 2>&1 || die "pkg not found — this is not a Termux shell."
ok "Termux environment detected ($PREFIX)"

step "Step 1/4: Termux packages (proot-distro, Termux:X11)"
pkg update -y || warn "pkg update had problems — continuing with what is installed"
pkg install -y x11-repo || warn "x11-repo install failed — termux-x11-nightly may not be found"
pkg install -y proot-distro || die "proot-distro install failed"
pkg install -y termux-x11-nightly || warn "termux-x11-nightly install failed — the X server command may be missing"
pkg install -y xorg-xdpyinfo || warn "xorg-xdpyinfo install failed — the start script will fall back to a socket-file check"
command -v proot-distro >/dev/null 2>&1 && ok "proot-distro installed ($(proot-distro --version 2>/dev/null || echo version unknown))" || die "proot-distro still missing"
command -v termux-x11 >/dev/null 2>&1 && ok "termux-x11 command installed" || warn "termux-x11 command not found — check the x11-repo step above"
# The Termux:X11 / Termux:Wayland Android apps are separate from the
# commands. pm is often NOT on PATH in Termux, so probe every route;
# any hit counts. (A single `command -v pm` check missed an installed
# app before — do not simplify this.)
app_detected() { # $1 = package name
  local pm_bin=""
  command -v pm >/dev/null 2>&1 && pm_bin="pm"
  [ -z "$pm_bin" ] && [ -x /system/bin/pm ] && pm_bin="/system/bin/pm"
  if [ -n "$pm_bin" ]; then
    "$pm_bin" list packages 2>/dev/null | grep -q "$1" && return 0
    "$pm_bin" path "$1" >/dev/null 2>&1 && return 0
  fi
  if command -v cmd >/dev/null 2>&1; then
    cmd package list packages 2>/dev/null | grep -q "$1" && return 0
    cmd package path "$1" >/dev/null 2>&1 && return 0
  fi
  [ -x /system/bin/cmd ] && /system/bin/cmd package list packages 2>/dev/null | grep -q "$1" && return 0
  return 1
}
if app_detected 'com\.termux\.x11'; then ok "Termux:X11 app detected"; else
  warn "Termux:X11 APP could not be confirmed from inside Termux (Android sometimes hides it)."
  warn "If it is installed, ignore this. If not: install the Termux:X11 app before starting the desktop."
fi
if app_detected 'com\.termux\.wayland'; then ok "Termux:Wayland app detected (for the later Wayland test)"; else
  warn "Termux:Wayland app not confirmed — only needed for the Wayland test path, not for X11."
fi

# Start scripts are placed FIRST, so they exist even if a later step
# fails and has to be re-run.
cp "$HERE/start-muxdesk.sh" "$HOME/start-muxdesk.sh" && chmod +x "$HOME/start-muxdesk.sh"
cp "$HERE/start-muxdesk-wayland.sh" "$HOME/start-muxdesk-wayland.sh" && chmod +x "$HOME/start-muxdesk-wayland.sh"
printf '%s' "$HERE" > "$HOME/.muxdesk-test-pkgdir"
ok "Start scripts placed: ~/start-muxdesk.sh (X11 test), ~/start-muxdesk-wayland.sh (Wayland test)"

step "Step 2/4: MuxDesk container in proot-distro"
# Ask proot-distro itself whether the container exists (its `list`),
# with a login probe as fallback. Never trust one hardcoded path:
# proot-distro 5.x keeps containers in containers/<name>/rootfs,
# older versions in installed-rootfs/<name>.
container_installed() {
  proot-distro list 2>/dev/null | grep -qw "$CONTAINER" && return 0
  [ -d "$PREFIX/var/lib/proot-distro/containers/$CONTAINER/rootfs" ] && return 0
  [ -d "$PREFIX/var/lib/proot-distro/installed-rootfs/$CONTAINER" ] && return 0
  return 1
}
if container_installed; then
  ok "MuxDesk container is already installed — keeping it (resume)"
else
  step "Step 3/4: Installing the MuxDesk rootfs image"
  IMAGE="${1:-${MUXDESK_IMAGE:-}}"
  TARBALL=""
  if [ -n "$IMAGE" ] && [ -f "$IMAGE" ]; then
    TARBALL="$IMAGE"
  elif [ -n "$IMAGE" ] && [ "${IMAGE#https://}" != "$IMAGE" ]; then
    TARBALL="$HOME/$(basename "$IMAGE")"
    echo "Downloading the MuxDesk image to $TARBALL (large — keep Wi-Fi on)..."
    curl -L -C - -o "$TARBALL" "$IMAGE" || die "download failed — check the URL and re-run (the download resumes)"
  else
    for d in "$HOME" "$HOME/storage/downloads" "$PWD"; do
      for f in "$d"/muxdesk-rootfs-aarch64-*.tar.gz "$d"/muxdesk-rootfs-aarch64-*.tar.xz "$d"/muxos-*.tar.gz "$d"/muxos-*.tar.xz "$d"/muxdesk-*.tar.gz; do
        [ -f "$f" ] && { TARBALL="$f"; break 2; }
      done
    done
    [ -n "$TARBALL" ] || die "No MuxDesk rootfs image found. Download muxdesk-rootfs-aarch64-*.tar.* from the muxdesk-repo Releases, put it in ~/ or ~/storage/downloads, or pass its path/URL: bash muxdesk-termux-setup.sh <path-or-url>"
  fi
  ok "Image: $TARBALL ($(du -h "$TARBALL" | cut -f1))"
  echo "Installing into proot-distro as container '$CONTAINER' (this extracts the full image — be patient)..."
  proot-distro install "$TARBALL" --name "$CONTAINER" || warn "proot-distro install reported an error — verifying before giving up"
  if ! container_installed; then
    if proot-distro login "$CONTAINER" -- /bin/true 2>/dev/null; then
      ok "MuxDesk container verified by login probe"
    else
      die "The MuxDesk container was not created. Run 'proot-distro install \"$TARBALL\" --name muxdesk' manually, read its error, then re-run this script."
    fi
  fi
  ok "MuxDesk container installed"
fi

step "Step 4/4: Checking the session entry inside the distro"
if proot-distro login "$CONTAINER" -- /bin/sh -c 'test -x /usr/local/bin/run-desktop || test -x /usr/local/bin/muxlike-session || test -x /usr/bin/labwc' 2>/dev/null; then
  ok "Session entry found inside the distro (run-desktop / muxlike-session / labwc)"
else
  warn "No session entry found inside the distro yet — the image may still be the bare base."
  warn "The start script will tell you the same. (The DE stack is the image builders' part.)"
fi

echo ""
echo "============================================================"
echo " SETUP DONE. To test the MuxDesk DE (X11 path):"
echo ""
echo "   1. Run:        bash ~/start-muxdesk.sh"
echo "   2. Then open:  the Termux:X11 app"
echo ""
echo " Wayland path (experimental, see README):"
echo "   bash ~/start-muxdesk-wayland.sh  + open the Termux:Wayland app"
echo "============================================================"
