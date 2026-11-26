#!/data/data/com.termux/files/usr/bin/bash
# MuxLike DE — start the test desktop (run in Termux, after setup).
# Starts the Termux:X11 server, then labwc inside the Debian proot.
set -u
step() { echo "==> $*"; }
fail() { echo "[FAIL] $*"; }

command -v termux-x11 >/dev/null 2>&1 || { fail "termux-x11 command not found — run the setup script first: bash muxlike-termux-test.sh"; exit 1; }
command -v proot-distro >/dev/null 2>&1 || { fail "proot-distro not found — run the setup script first"; exit 1; }

step "Starting Termux:X11 server on :0"
termux-x11 :0 >/dev/null 2>&1 &
X11_PID=$!
sleep 2

step "Starting MuxLike DE inside Debian (labwc, nested on X11)"
echo "Now open the Termux:X11 app to see the desktop."
echo "To stop the desktop: exit labwc (panel logo menu) or press Ctrl+C here."
proot-distro login debian --shared-tmp -- /bin/bash /root/.local/bin/start-muxlike-session
RC=$?

kill "$X11_PID" 2>/dev/null || true
echo "Desktop stopped (exit $RC). Start again any time: bash ~/start-muxlike-test.sh"
