#!/bin/sh
# MuxOS distro — start the DE session INSIDE the container.
# Called by the Termux start scripts through proot-distro.
#   $1 = x11     -> labwc nested on the X11 backend (Termux:X11)
#   $1 = wayland -> labwc nested on the Wayland backend (Termux:Wayland)
# Uses the image's OWN session chain: run-desktop (reads
# /etc/muxdesk-session) first, then muxlike-session, then plain labwc.
set -u
MODE="${1:-x11}"
export PATH="$HOME/.local/bin:/usr/local/bin:$PATH"

# Run under a session bus when one is available (our panel's global
# menu and the apps need it). Re-exec self once, guarded by a flag.
if [ -z "${MUXDESK_DBUS_DONE:-}" ] && command -v dbus-run-session >/dev/null 2>&1; then
  export MUXDESK_DBUS_DONE=1
  exec dbus-run-session -- /bin/sh "$0" "$MODE"
fi

# labwc/wlroots need a runtime dir they can create sockets in (0700),
# and a writable shm (the start script binds one to /dev/shm).
export XDG_RUNTIME_DIR="$HOME/.muxdesk-runtime"
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"

export WLR_RENDERER=pixman
export LIBGL_ALWAYS_SOFTWARE=1
export GSK_RENDERER=cairo
export XDG_CURRENT_DESKTOP=MuxLike
export XDG_SESSION_TYPE=wayland

# Timezone: the start script writes the phone's zone (Android
# persist.sys.timezone) into tz.env in the bind-mounted package dir;
# the image has tzdata but no /etc/localtime, so without this the
# panel clock shows UTC.
if [ -z "${TZ:-}" ] && [ -f /root/muxdesk-test/tz.env ]; then
  TZ="$(tr -d ' \r\n' < /root/muxdesk-test/tz.env)"
  [ -n "$TZ" ] && export TZ
fi

# Theme: the image ships our MuxLike icon theme but nothing selects
# it, and GTK defaults to light — on the phone Files came up white
# with stock blue folder icons. Write the GTK settings ONCE (never
# overwrite an existing file): dark variant + MuxLike icons.
for V in gtk-4.0 gtk-3.0; do
  INI="$HOME/.config/$V/settings.ini"
  if [ ! -f "$INI" ]; then
    mkdir -p "$HOME/.config/$V"
    {
      echo '[Settings]'
      echo 'gtk-application-prefer-dark-theme=1'
      [ -d /usr/share/icons/MuxLike ] && echo 'gtk-icon-theme-name=MuxLike'
      [ -d /usr/share/icons/default/cursors ] && echo 'gtk-cursor-theme-name=default'
    } > "$INI"
    echo "[test-path] Wrote $INI (dark theme + MuxLike icons)."
  fi
done

if [ "$MODE" = "wayland" ]; then
  # Termux:Wayland's socket dir is bind-mounted at /root/.wayland-host
  # by the start script; it becomes the runtime dir so labwc finds the
  # parent compositor AND can place its own socket beside it.
  if [ -d /root/.wayland-host ]; then
    export XDG_RUNTIME_DIR=/root/.wayland-host
    WL_SOCK="$(ls /root/.wayland-host 2>/dev/null | grep -E '^wayland-[0-9]+$' | head -1)"
    [ -n "$WL_SOCK" ] && export WAYLAND_DISPLAY="$WL_SOCK"
  fi
  export WLR_BACKENDS=wayland
  echo "Starting the MuxOS session (labwc nested on Wayland, WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-unset}) ..."
else
  export DISPLAY="${DISPLAY:-:0}"
  export WLR_BACKENDS=x11
  echo "Starting the MuxOS session (labwc nested on X11, DISPLAY=$DISPLAY) ..."
fi
echo "Open the Termux:X11 app (X11 test) or the Termux:Wayland app (Wayland test) to see the desktop."

if [ -x /usr/local/bin/run-desktop ]; then
  echo "Session entry: run-desktop ($(cat /etc/muxdesk-session 2>/dev/null || echo 'no /etc/muxdesk-session'))"
  exec /usr/local/bin/run-desktop
elif [ -x /usr/lib/muxdesk/run-desktop.sh ] && grep -q muxlike /usr/lib/muxdesk/run-desktop.sh 2>/dev/null; then
  # The v4.0 image carries an older run-desktop at /usr/lib/muxdesk/
  # with NO muxlike case (it would try GNOME, which the image does
  # not contain) — use it only once it actually knows muxlike.
  echo "Session entry: /usr/lib/muxdesk/run-desktop.sh (muxlike)"
  exec /usr/lib/muxdesk/run-desktop.sh
elif [ -x /usr/local/bin/muxlike-session ]; then
  echo "Session entry: muxlike-session"
  exec /usr/local/bin/muxlike-session
elif command -v labwc >/dev/null 2>&1; then
  echo "Session entry: labwc (direct — no run-desktop in this image)"
  # v4.0 image gap (verified from the published tarball): mux-panel,
  # mux-dock and swaybg ARE installed (/usr/bin) but the image has NO
  # labwc autostart and no session wiring — plain labwc = black
  # screen with only a cursor (seen on the phone, 2026-11-27).
  # Test-path fallback: write a minimal autostart ONCE (never
  # overwrite an existing one) so the test shows the actual DE.
  CFG_DIR="$HOME/.config/labwc"
  mkdir -p "$CFG_DIR"
  # Wallpaper (Appearance contract): the user's chosen wallpaper from
  # ~/.config/muxdesk/appearance.conf wins; else our bundled MuxLike
  # gradient (shipped in this package, bind-mounted at
  # /root/muxdesk-test/wallpaper.png); else a solid color.
  WP=""
  if [ -f "$HOME/.config/muxdesk/appearance.conf" ]; then
    WP="$(sed -n 's/^[[:space:]]*wallpaper[[:space:]]*=[[:space:]]*//p' "$HOME/.config/muxdesk/appearance.conf" | head -1 | tr -d '\r')"
    if [ -n "$WP" ] && [ ! -f "$WP" ]; then WP=""; fi
  fi
  if [ -z "$WP" ] && [ -f /root/muxdesk-test/wallpaper.png ]; then WP=/root/muxdesk-test/wallpaper.png; fi
  if [ ! -f "$CFG_DIR/autostart" ] && { command -v mux-panel >/dev/null 2>&1 || command -v mux-dock >/dev/null 2>&1; }; then
    {
      echo '# Written by the MuxOS test path (inside-start.sh) because'
      echo '# this image ships no labwc autostart. Safe to edit/remove.'
      if command -v swaybg >/dev/null 2>&1; then
        if [ -n "$WP" ]; then echo "swaybg -i '$WP' -m fill >/dev/null 2>&1 &"; else echo "swaybg -c '#1d1033' >/dev/null 2>&1 &"; fi
      fi
      command -v mux-panel >/dev/null 2>&1 && echo 'mux-panel >/dev/null 2>&1 &'
      command -v mux-dock >/dev/null 2>&1 && echo 'mux-dock >/dev/null 2>&1 &'
    } > "$CFG_DIR/autostart"
    chmod +x "$CFG_DIR/autostart"
    echo "[test-path] Image has panel/dock but no session wiring — wrote a fallback labwc autostart (wallpaper + mux-panel + mux-dock)."
  elif [ -f "$CFG_DIR/autostart" ] && [ -n "$WP" ] && grep -q 'Written by the MuxOS test path' "$CFG_DIR/autostart"; then
    # Upgrade OUR OWN earlier fallback (v4 wrote a solid-color swaybg
    # line): swap just that line for the real wallpaper. A hand-edited
    # or image-shipped autostart is never touched.
    sed -i "s|^swaybg -c '#1d1033'.*|swaybg -i '$WP' -m fill >/dev/null 2>\\&1 \\&|" "$CFG_DIR/autostart"
  fi
  # Root (right-click) menu: the stock labwc menu (Terminal /
  # Reconfigure / Exit) is not ours. Write OUR menu.xml once — items
  # only for things that actually exist in this image.
  if [ ! -f "$CFG_DIR/menu.xml" ]; then
    {
      echo '<?xml version="1.0" encoding="UTF-8"?>'
      echo '<openbox_menu>'
      echo '  <menu id="root-menu" label="MuxOS">'
      command -v mux-settings >/dev/null 2>&1 && echo '    <item label="About MuxOS"><action name="Execute"><command>mux-settings</command></action></item>'
      echo '    <item label="New Folder"><action name="Execute"><command>sh /root/muxdesk-test/new-folder.sh</command></action></item>'
      echo '    <item label="Refresh"><action name="Reconfigure" /></item>'
      command -v mux-appearance >/dev/null 2>&1 && echo '    <item label="Change Wallpaper"><action name="Execute"><command>mux-appearance</command></action></item>'
      for T in mux-terminal foot alacritty kitty wezterm xterm; do
        if command -v "$T" >/dev/null 2>&1; then echo "    <item label=\"Open Terminal\"><action name=\"Execute\"><command>$T</command></action></item>"; break; fi
      done
      echo '    <separator />'
      echo '    <item label="Exit"><action name="Exit" /></item>'
      echo '  </menu>'
      echo '</openbox_menu>'
    } > "$CFG_DIR/menu.xml"
    echo "[test-path] Wrote our labwc root menu (menu.xml) — right-click the desktop."
  fi
  exec labwc
else
  echo "[FAIL] No session entry inside this image: no run-desktop, no muxlike-session, no labwc."
  echo "This image is the bare base without the DE stack — that part belongs to the image builders."
  exit 1
fi
