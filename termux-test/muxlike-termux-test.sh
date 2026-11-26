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
# Detection must be thorough: in Termux, `pm` is often NOT on PATH
# (v2 first cut missed an installed app exactly that way), so probe the
# package manager through every route — PATH pm, /system/bin/pm,
# `cmd package`, and `pm path`. Any single hit means the app is there.
x11_app_detected() {
  local pm_bin=""
  command -v pm >/dev/null 2>&1 && pm_bin="pm"
  [ -z "$pm_bin" ] && [ -x /system/bin/pm ] && pm_bin="/system/bin/pm"
  if [ -n "$pm_bin" ]; then
    "$pm_bin" list packages 2>/dev/null | grep -q 'com\.termux\.x11' && return 0
    "$pm_bin" path com.termux.x11 >/dev/null 2>&1 && return 0
  fi
  if command -v cmd >/dev/null 2>&1; then
    cmd package list packages 2>/dev/null | grep -q 'com\.termux\.x11' && return 0
    cmd package path com.termux.x11 >/dev/null 2>&1 && return 0
  fi
  [ -x /system/bin/cmd ] && /system/bin/cmd package list packages 2>/dev/null | grep -q 'com\.termux\.x11' && return 0
  return 1
}
if x11_app_detected; then
  ok "Termux:X11 app detected"
else
  warn "Termux:X11 APP could not be confirmed from inside Termux (Android sometimes hides it)."
  warn "If you already installed the Termux:X11 app, ignore this. If not: install it"
  warn "(APK from the Termux:X11 GitHub releases / F-Droid) before starting the desktop."
fi

# The start script is created FIRST, so it exists even if a later step
# fails and has to be re-run (v1 bug: it was only created at the end).
cp "$HERE/start-muxlike-test.sh" "$HOME/start-muxlike-test.sh" && chmod +x "$HOME/start-muxlike-test.sh"
ok "Start script placed: ~/start-muxlike-test.sh (ready once setup completes)"

step "Step 2/4: Debian proot"
# Where is the Debian rootfs? DO NOT hardcode one layout:
#   proot-distro 5.x:  $PREFIX/var/lib/proot-distro/containers/debian/rootfs
#   proot-distro <=4:  $PREFIX/var/lib/proot-distro/installed-rootfs/debian (legacy)
# Detection asks proot-distro itself first (its `list`), then the paths,
# then a login probe (which also triggers the legacy->new migration).
PD_DIR="$PREFIX/var/lib/proot-distro"
find_debian_rootfs() {
  local d
  for d in "$PD_DIR/containers/debian/rootfs" "$PD_DIR/installed-rootfs/debian"; do
    if [ -d "$d" ]; then printf '%s' "$d"; return 0; fi
  done
  return 1
}
debian_installed() {
  find_debian_rootfs >/dev/null 2>&1 && return 0
  proot-distro list 2>/dev/null | grep -qw 'debian' && return 0
  return 1
}
echo "proot-distro version: $(proot-distro --version 2>/dev/null || echo unknown)"
if debian_installed; then
  ok "Debian is already installed — keeping it (resume)"
else
  echo "Installing Debian (this downloads ~100-200 MB and can take a while)..."
  proot-distro install debian || warn "proot-distro install reported an error — verifying before giving up"
  if ! debian_installed; then
    # Last resort: a login probe — succeeds only if the container really
    # exists, and migrates a legacy-layout rootfs to the new layout.
    if proot-distro login debian -- /bin/true 2>/dev/null; then
      ok "Debian verified by login probe"
    else
      die "Debian is not installed and the install did not create it. Run 'proot-distro install debian' manually, read its error, then re-run this script."
    fi
  fi
  ok "Debian installed"
fi
ROOTFS_DIR="$(find_debian_rootfs || true)"
[ -n "$ROOTFS_DIR" ] && ok "Debian rootfs found at: $ROOTFS_DIR" \
  || warn "Rootfs path not visible from Termux — the package will be bind-mounted into Debian instead of copied (same result)"

step "Step 3/4: Getting the MuxLike package into Debian"
BIND_ARGS=""
if [ -n "$ROOTFS_DIR" ]; then
  DEST="$ROOTFS_DIR/root/muxlike-test"
  rm -rf "$DEST"; mkdir -p "$DEST"
  cp -r "$HERE/." "$DEST/" || die "copy into Debian rootfs failed"
  ok "Package copied to Debian: /root/muxlike-test"
else
  BIND_ARGS="--bind $HERE:/root/muxlike-test"
  ok "Package will be bind-mounted at /root/muxlike-test"
fi

step "Step 4/4: Building and installing inside Debian (packages + our code)"
echo "This is the long step: apt packages, then compiling panel/dock/launcher."
# shellcheck disable=SC2086
proot-distro login $BIND_ARGS debian -- /bin/bash /root/muxlike-test/inside-debian.sh
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
