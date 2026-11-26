#!/bin/bash
# MuxLike DE — inside-Debian setup. Runs as root inside the Debian proot.
# Installs packages, builds our C components from the bundled sources,
# and installs configs/theme/icons/apps into this user's home.
# Safe to re-run. Set MUXLIKE_SKIP_APT=1 to skip the apt step (testing).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
B="$HERE/bundle"
step() { echo ""; echo "==> $*"; }
ok()   { echo "[ OK ] $*"; }
warn() { echo "[WARN] $*"; }
fail() { echo "[FAIL] $*"; }
FAILED=0

[ "$(id -u)" = 0 ] || { fail "inside-debian.sh must run as root inside Debian (proot default user)"; exit 1; }
export DEBIAN_FRONTEND=noninteractive
BIN="$HOME/.local/bin"; mkdir -p "$BIN"
export PATH="$BIN:/usr/local/bin:$PATH"

step "Debian packages"
if [ "${MUXLIKE_SKIP_APT:-0}" = 1 ]; then
  warn "MUXLIKE_SKIP_APT=1 — apt step skipped (test mode)"
else
  apt-get update || { fail "apt-get update failed — check the network inside Termux"; exit 1; }
  # Runtime: labwc (Debian trixie ships 0.8.3), wallpaper, terminal, our apps' GTK4 stack, session bus
  apt-get install -y --no-install-recommends \
    labwc swaybg foot dbus \
    python3-gi gir1.2-gtk-4.0 libgtk-4-1 \
    gir1.2-gtk4layershell-1.0 libgtk4-layer-shell0 \
    || { fail "runtime package install failed"; exit 1; }
  ok "runtime packages installed"
  # Build tools + dev headers for our C panel/dock/launcher
  apt-get install -y --no-install-recommends \
    build-essential pkg-config meson ninja-build wayland-protocols \
    libwayland-dev libcairo2-dev libpango1.0-dev libglib2.0-dev \
    libgtk-4-dev libgtk4-layer-shell-dev \
    || { fail "build dependency install failed"; exit 1; }
  ok "build dependencies installed"
  # Optional extras — the desktop works without them
  apt-get install -y --no-install-recommends mako-notifier grim wl-clipboard xwayland \
    || warn "some optional packages missing (mako/grim/wl-clipboard/xwayland) — continuing"
fi
command -v labwc >/dev/null 2>&1 && ok "labwc: $(labwc --version 2>/dev/null | head -1)" || fail "labwc not found after install"

step "Building our C panel and dock (canonical sources, bundled)"
for comp in panel dock; do
  name="mux-$comp"
  if ( cd "$B/components/$comp" && make >/tmp/build-$comp.log 2>&1 ); then
    cp "$B/components/$comp/build/$name" "$BIN/$name" && chmod +x "$BIN/$name"
    ok "$name built and installed"
  else
    fail "$name build failed — log: /tmp/build-$comp.log (inside Debian)"; FAILED=1
  fi
done

step "Building our C launcher (GTK4 + gtk4-layer-shell)"
if ( cd "$B/components/launcher" && rm -rf build && meson setup build >/tmp/build-launcher.log 2>&1 && meson compile -C build >>/tmp/build-launcher.log 2>&1 ); then
  cp "$B/components/launcher/build/mux-launcher" "$BIN/mux-launcher" && chmod +x "$BIN/mux-launcher"
  ok "mux-launcher built and installed"
else
  fail "mux-launcher build failed — log: /tmp/build-launcher.log (inside Debian)."
  fail "The desktop still starts; Super key / launcher button will not work until this builds."
  FAILED=1
fi

step "Installing labwc config, theme, icons, wallpaper"
mkdir -p "$HOME/.config/labwc"
cp "$B/labwc/rc.xml" "$B/labwc/menu.xml" "$B/labwc/environment" "$B/labwc/autostart" "$HOME/.config/labwc/"
chmod +x "$HOME/.config/labwc/autostart"
ok "labwc config installed (autostart starts our panel + dock — no waybar)"
mkdir -p "$HOME/.local/share/themes" "$HOME/.local/share/icons"
rm -rf "$HOME/.local/share/themes/MuxLike" "$HOME/.local/share/icons/MuxLike"
cp -r "$B/theme/MuxLike" "$HOME/.local/share/themes/MuxLike"
cp -r "$B/icons/MuxLike" "$HOME/.local/share/icons/MuxLike"
ok "MuxLike theme + icon theme installed"
mkdir -p /usr/share/backgrounds/muxdesk "$HOME/.local/share/backgrounds/muxlike"
cp "$B/wallpaper/muxlike-gradient.svg" /usr/share/backgrounds/muxdesk/ 2>/dev/null \
  || cp "$B/wallpaper/muxlike-gradient.svg" "$HOME/.local/share/backgrounds/muxlike/"
ok "wallpaper installed"

step "Installing shell configs and our apps (Mux Files / Mux Settings)"
mkdir -p "$HOME/.config/muxlike" "$HOME/.config/muxdesk"
cp "$B/components/panel/config/panel.conf" "$HOME/.config/muxlike/panel.conf"
cp "$B/components/dock/config/dock.conf" "$HOME/.config/muxlike/dock.conf"
cp "$B/components/launcher/config/mux-launcher.ini" "$HOME/.config/muxdesk/mux-launcher.ini"
mkdir -p "$HOME/.local/share/muxdesk" "$HOME/.local/share/applications"
rm -rf "$HOME/.local/share/muxdesk/common" "$HOME/.local/share/muxdesk/mux-files" "$HOME/.local/share/muxdesk/mux-settings"
cp -r "$B/apps/common" "$HOME/.local/share/muxdesk/common"
for a in mux-files mux-settings; do
  cp -r "$B/apps/$a" "$HOME/.local/share/muxdesk/$a"
  cp "$B/apps/$a/$a" "$BIN/$a" && chmod +x "$BIN/$a"
done
cp "$B"/apps/mux-*.desktop "$HOME/.local/share/applications/"
cp "$B/components/launcher/data/mux-launcher.desktop" "$HOME/.local/share/applications/" 2>/dev/null || true
ok "Mux Files + Mux Settings installed"

step "Installing the inside-Debian start command"
cp "$HERE/inside-start.sh" "$BIN/start-muxlike-session" && chmod +x "$BIN/start-muxlike-session"
ok "start command: start-muxlike-session (used by the Termux start script)"

echo ""
echo "============================================================"
echo " Inside-Debian setup finished."
[ "$FAILED" = 1 ] && echo " Some builds FAILED — see [FAIL] lines above." || echo " All components built and installed."
echo " Back in Termux, start with:  bash ~/start-muxlike-test.sh"
echo "============================================================"
exit "$FAILED"
