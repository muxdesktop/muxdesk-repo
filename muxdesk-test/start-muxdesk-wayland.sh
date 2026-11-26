#!/data/data/com.termux/files/usr/bin/bash
# MuxOS distro — Wayland test start (run in Termux, after setup).
# EXPERIMENTAL: enters the MuxDesk container via proot-distro and
# starts the DE session nested on labwc's WAYLAND backend, displayed
# by the Termux:Wayland app. The X11 path (start-muxdesk.sh) is the
# primary, better-tested one — see README.
set -u
PKG_DIR="$(cat "$HOME/.muxdesk-test-pkgdir" 2>/dev/null || true)"
if [ -z "$PKG_DIR" ] || [ ! -f "$PKG_DIR/inside-start.sh" ]; then
  for d in "$HOME/muxdesk-test" "$PWD/muxdesk-test" "$(cd "$(dirname "$0")" && pwd)"; do
    [ -f "$d/inside-start.sh" ] && { PKG_DIR="$d"; break; }
  done
fi
[ -n "$PKG_DIR" ] && [ -f "$PKG_DIR/inside-start.sh" ] || { echo "[FAIL] Cannot find the test package folder (inside-start.sh). Re-run the setup script from the unpacked package folder."; exit 1; }
CONTAINER="muxdesk"
step() { echo "==> $*"; }
ok()   { echo "[ OK ] $*"; }
fail() { echo "[FAIL] $*"; }
SHM_DIR="$PREFIX/tmp/muxdesk-shm"

command -v proot-distro >/dev/null 2>&1 || { fail "proot-distro not found — run the setup script first"; exit 1; }
proot-distro list 2>/dev/null | grep -qw "$CONTAINER" || { fail "MuxDesk container not installed — run the setup script first."; exit 1; }

step "Looking for the Termux:Wayland compositor socket"
echo "The Termux:Wayland app must be OPEN (it creates the socket when it runs)."
WL_DIR=""
for d in "${XDG_RUNTIME_DIR:-}" "$PREFIX/tmp" "$PREFIX/var/run"; do
  [ -n "$d" ] || continue
  for s in "$d"/wayland-*; do
    [ -S "$s" ] && { WL_DIR="$d"; break 2; }
  done
done
if [ -z "$WL_DIR" ]; then
  fail "No Wayland socket found in \$XDG_RUNTIME_DIR, $PREFIX/tmp or $PREFIX/var/run."
  echo "Open the Termux:Wayland app, leave it running, come back here and run this script again."
  exit 1
fi
ok "Wayland socket dir: $WL_DIR"
mkdir -p "$SHM_DIR"

step "Starting the MuxDesk DE session (labwc nested on Wayland)"
echo "Switch to the Termux:Wayland app to see the desktop."
echo "To stop: exit the session from inside, or press Ctrl+C here."
proot-distro login \
  --bind "$SHM_DIR:/dev/shm" \
  --bind "$WL_DIR:/root/.wayland-host" \
  --bind "$PKG_DIR:/root/muxdesk-test" \
  "$CONTAINER" -- /bin/bash /root/muxdesk-test/inside-start.sh wayland
RC=$?
echo "Desktop stopped (exit $RC). Start again any time: bash ~/start-muxdesk-wayland.sh"
