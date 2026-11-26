#!/data/data/com.termux/files/usr/bin/bash
# MuxLike DE — Termux test setup (main script, run this in Termux)
#
# What it does:
#   1. Installs proot-distro + Termux:X11 support in Termux
#   2. Installs a Debian proot (if missing)
#   3. Copies this package into Debian and runs inside-debian.sh there
#      (packages, builds our C panel/dock/launcher, installs configs/apps)
#   4. Leaves a start script:  ~/start-muxlike-test.sh
#
# Honest note: this is a NESTED TEST RIG — labwc runs on the X11 backend
# inside Termux:X11. It is NOT the MuxLike app's native Wayland path.
# Input/rendering quirks are expected. See README.md.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
step() { echo ""; echo "==> $*"; }
ok()   { echo "[ OK ] $*"; }
warn() { echo "[WARN] $*"; }
fail() { echo "[FAIL] $*"; }
die()  { fail "$*"; echo "Setup stopped. Fix the problem above and run this script again — it is safe to re-run."; exit 1; }

step "MuxLike DE Termux test setup"
[ -n "${PREFIX:-}" ] && [ -d "$PREFIX" ] || die "This script must run inside Termux (PREFIX is not set)."
command -v pkg >/dev/null 2>&1 || die "pkg not found — this is not a Termux shell."
ok "Termux environment detected ($PREFIX)"

step "Step 1/4: Termux packages (proot-distro, Termux:X11)"
pkg update -y || warn "pkg update had problems — continuing with what is installed"
pkg install -y x11-repo || warn "x11-repo install failed — termux-x11-nightly may not be found"
pkg install -y proot-distro || die "proot-distro install failed"
pkg install -y termux-x11-nightly || warn "termux-x11-nightly install failed — the X server command may be missing"
command -v proot-distro >/dev/null 2>&1 && ok "proot-distro installed" || die "proot-distro still missing"
command -v termux-x11 >/dev/null 2>&1 && ok "termux-x11 command installed" || warn "termux-x11 command not found — check the x11-repo step above"
# The Termux:X11 Android app is a separate app from the command.
if command -v pm >/dev/null 2>&1 && pm list packages 2>/dev/null | grep -q 'com.termux.x11'; then
  ok "Termux:X11 app detected"
else
  warn "Termux:X11 APP not detected. The 'termux-x11' command alone is not enough:"
  warn "install the Termux:X11 app (APK from the Termux:X11 GitHub releases / F-Droid) before starting the desktop."
fi

step "Step 2/4: Debian proot"
ROOTFS_DIR="$PREFIX/var/lib/proot-distro/installed-rootfs/debian"
if [ -d "$ROOTFS_DIR" ]; then
  ok "Debian is already installed — keeping it"
else
  echo "Installing Debian (this downloads ~100-200 MB and can take a while)..."
  proot-distro install debian || die "proot-distro install debian failed"
  [ -d "$ROOTFS_DIR" ] && ok "Debian installed" || die "Debian rootfs not found after install"
fi

step "Step 3/4: Copying the MuxLike package into Debian"
DEST="$ROOTFS_DIR/root/muxlike-test"
rm -rf "$DEST"; mkdir -p "$DEST"
cp -r "$HERE/." "$DEST/" || die "copy into Debian rootfs failed"
ok "Package copied to Debian: /root/muxlike-test"

step "Step 4/4: Building and installing inside Debian (packages + our code)"
echo "This is the long step: apt packages, then compiling panel/dock/launcher."
proot-distro login debian -- /bin/bash /root/muxlike-test/inside-debian.sh
RC=$?
[ "$RC" = 0 ] && ok "Inside-Debian setup finished" || warn "Inside-Debian setup ended with warnings/failures (exit $RC) — read the [FAIL]/[WARN] lines above. The desktop may still start; the launcher is the most likely missing piece."

cp "$HERE/start-muxlike-test.sh" "$HOME/start-muxlike-test.sh" && chmod +x "$HOME/start-muxlike-test.sh"
ok "Start script ready: ~/start-muxlike-test.sh"

echo ""
echo "============================================================"
echo " SETUP DONE. To see the MuxLike desktop:"
echo ""
echo "   1. Run:        bash ~/start-muxlike-test.sh"
echo "   2. Then open:  the Termux:X11 app"
echo ""
echo " The desktop (labwc + our panel + dock + wallpaper) appears"
echo " inside Termux:X11. To stop: close labwc from the panel menu"
echo " or press Ctrl+C in the Termux window running the start script."
echo "============================================================"
