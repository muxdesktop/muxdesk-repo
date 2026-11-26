#!/bin/sh
# MuxLike DE — start the desktop INSIDE Debian (called by the Termux
# start script through proot-distro --shared-tmp, so DISPLAY=:0 reaches
# the Termux:X11 server socket in the shared /tmp).
#
# labwc runs NESTED on wlroots' X11 backend — this is a test rig, not
# the native Wayland path of the MuxLike app. Expect quirks.
set -u
export PATH="$HOME/.local/bin:/usr/local/bin:$PATH"

# labwc needs a runtime dir it can create sockets in (0700).
export XDG_RUNTIME_DIR="$HOME/.muxlike-runtime"
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"

export DISPLAY="${DISPLAY:-:0}"
export WLR_BACKENDS=x11
export WLR_RENDERER=pixman
export LIBGL_ALWAYS_SOFTWARE=1
export GSK_RENDERER=cairo
export XDG_CURRENT_DESKTOP=MuxLike
export XDG_SESSION_TYPE=wayland

echo "Starting labwc (nested, X11 backend) on DISPLAY=$DISPLAY ..."
echo "Open the Termux:X11 app to see the desktop."
if command -v dbus-run-session >/dev/null 2>&1; then
  exec dbus-run-session -- labwc
else
  exec labwc
fi
