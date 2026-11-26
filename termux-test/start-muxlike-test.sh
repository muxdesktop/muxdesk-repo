#!/data/data/com.termux/files/usr/bin/bash
# MuxLike DE — start the test desktop (run in Termux, after setup).
# Starts the Termux:X11 server, WAITS until its socket is really up,
# then starts labwc inside the Debian proot.
set -u
step() { echo "==> $*"; }
ok()   { echo "[ OK ] $*"; }
fail() { echo "[FAIL] $*"; }
X11_LOG="$HOME/muxlike-x11.log"
SOCK_DIR="$PREFIX/tmp/.X11-unix"
SOCK="$SOCK_DIR/X0"

command -v termux-x11 >/dev/null 2>&1 || { fail "termux-x11 command not found — run the setup script first: bash muxlike-termux-test.sh"; exit 1; }
command -v proot-distro >/dev/null 2>&1 || { fail "proot-distro not found — run the setup script first"; exit 1; }

# Is the inside-Debian setup actually finished? (proot-distro is the
# source of truth — no hardcoded rootfs paths here either.)
if ! proot-distro login debian -- /bin/test -x /root/.local/bin/start-muxlike-session 2>/dev/null; then
  fail "The MuxLike setup inside Debian is not finished yet."
  echo "Run the setup script again — it resumes where it stopped:"
  echo "  bash muxlike-termux-test/muxlike-termux-test.sh"
  exit 1
fi

step "Starting Termux:X11 server on :0"
# The socket directory must exist BEFORE proot starts, or proot drops
# the bind with a warning and labwc cannot reach the X server
# ("Failed to open xcb connection" — the v2 bug).
mkdir -p "$SOCK_DIR"
termux-x11 :0 >"$X11_LOG" 2>&1 &
X11_PID=$!

# Wait for the server to be REALLY ready (v2 only slept 2s — on a busy
# phone the socket was not there yet when labwc started).
READY=0
for i in $(seq 1 30); do
  if [ -S "$SOCK" ]; then
    if command -v xdpyinfo >/dev/null 2>&1; then
      DISPLAY=:0 xdpyinfo >/dev/null 2>&1 && { READY=1; break; }
    else
      READY=1; break
    fi
  fi
  # Our server process died but a socket exists: an earlier server is
  # still holding :0 — that is fine, use it.
  if ! kill -0 "$X11_PID" 2>/dev/null && [ -S "$SOCK" ]; then READY=1; break; fi
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

step "Starting MuxLike DE inside Debian (labwc, nested on X11)"
echo "Now open the Termux:X11 app to see the desktop."
echo "To stop the desktop: exit labwc (panel logo menu) or press Ctrl+C here."
# --shared-tmp only: it carries /tmp/.X11-unix with it. (v2 also passed
# --shared-x11; when its source dir was missing proot dropped that bind
# noisily — one bind, the proven one, is enough.)
proot-distro login --shared-tmp debian -- /bin/bash /root/.local/bin/start-muxlike-session
RC=$?

kill "$X11_PID" 2>/dev/null || true
echo "Desktop stopped (exit $RC). Start again any time: bash ~/start-muxlike-test.sh"
