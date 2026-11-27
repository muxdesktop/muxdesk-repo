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
  if [ ! -f "$CFG_DIR/autostart" ] && { command -v mux-panel >/dev/null 2>&1 || command -v mux-dock >/dev/null 2>&1; }; then
    mkdir -p "$CFG_DIR"
    {
      echo '# Written by the MuxOS test path (inside-start.sh) because'
      echo '# this image ships no labwc autostart. Safe to edit/remove.'
      command -v swaybg >/dev/null 2>&1 && echo "swaybg -c '#1d1033' >/dev/null 2>&1 &"
      command -v mux-panel >/dev/null 2>&1 && echo 'mux-panel >/dev/null 2>&1 &'
      command -v mux-dock >/dev/null 2>&1 && echo 'mux-dock >/dev/null 2>&1 &'
    } > "$CFG_DIR/autostart"
    chmod +x "$CFG_DIR/autostart"
    echo "[test-path] Image has panel/dock but no session wiring — wrote a fallback labwc autostart (swaybg + mux-panel + mux-dock)."
  fi
  exec labwc
else
  echo "[FAIL] No session entry inside this image: no run-desktop, no muxlike-session, no labwc."
  echo "This image is the bare base without the DE stack — that part belongs to the image builders."
  exit 1
fi
