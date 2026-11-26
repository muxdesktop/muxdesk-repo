#!/data/data/com.termux/files/usr/bin/bash
# MuxOS distro — X11 test start (run in Termux, after setup).
# Starts Termux:X11, WAITS until its socket really answers, then enters
# the MuxOS container via proot-distro and starts the DE session
# nested on labwc's X11 backend.
set -u
# The package dir (holds inside-start.sh) is recorded by the setup
# script; fall back to the usual unpack locations.
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
X11_LOG="$HOME/muxdesk-x11.log"
SOCK_DIR="$PREFIX/tmp/.X11-unix"
SOCK="$SOCK_DIR/X0"
SHM_DIR="$PREFIX/tmp/muxdesk-shm"

command -v termux-x11 >/dev/null 2>&1 || { fail "termux-x11 command not found — run the setup script first: bash muxdesk-termux-setup.sh"; exit 1; }
command -v proot-distro >/dev/null 2>&1 || { fail "proot-distro not found — run the setup script first"; exit 1; }
proot-distro list 2>/dev/null | grep -qw "$CONTAINER" || { fail "MuxOS container not installed — run the setup script first."; exit 1; }

step "Starting Termux:X11 server on :0"
# The socket directory must exist BEFORE proot starts, or proot drops
# the bind and labwc cannot reach the X server ("Failed to open xcb
# connection"). The shm dir is bound to /dev/shm inside the container:
# wlroots needs a writable shm for the keyboard keymap (proven on the
# phone with the MuxLike app — without it labwc exits).
mkdir -p "$SOCK_DIR" "$SHM_DIR"
termux-x11 :0 >"$X11_LOG" 2>&1 &
X11_PID=$!

READY=0
for i in $(seq 1 30); do
  if [ -S "$SOCK" ]; then
    if command -v xdpyinfo >/dev/null 2>&1; then
      DISPLAY=:0 xdpyinfo >/dev/null 2>&1 && { READY=1; break; }
    else
      READY=1; break
    fi
  fi
  if ! kill -0 "$X11_PID" 2>/dev/null && [ -S "$SOCK" ]; then READY=1; break; fi  # earlier server holds :0 — use it
  if ! kill -0 "$X11_PID" 2>/dev/null && [ ! -S "$SOCK" ] && [ "$i" -gt 3 ]; then break; fi
  sleep 1
done
if [ "$READY" != 1 ]; then
  fail "The Termux:X11 server did not come up (no socket at $SOCK)."
  echo "--- termux-x11 log ($X11_LOG) ---"; tail -5 "$X11_LOG" 2>/dev/null
  echo "Try: open the Termux:X11 app once, close any other Termux session"
  echo "running an X server, then run this script again."
  kill "$X11_PID" 2>/dev/null || true
  exit 1
fi
ok "Termux:X11 server is up (socket $SOCK)"

step "Starting the MuxOS DE session (labwc nested on X11)"
echo "Now open the Termux:X11 app to see the desktop."
echo "To stop: exit the session from inside, or press Ctrl+C here."
proot-distro login --shared-tmp \
  --bind "$SHM_DIR:/dev/shm" \
  --bind "$PKG_DIR:/root/muxdesk-test" \
  "$CONTAINER" -- /bin/bash /root/muxdesk-test/inside-start.sh x11
RC=$?

kill "$X11_PID" 2>/dev/null || true
echo "Desktop stopped (exit $RC). Start again any time: bash ~/start-muxdesk.sh"
