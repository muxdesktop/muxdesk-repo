"""Global menu (Mac-ধাঁচের panel menu) — DBus export সহায়ক।

Panel (mux-panel) com.canonical.AppMenu.Registrar-এ RegisterWindow
পেলে window-এর menubar নিজের bar-এ দেখায়। এই module:
  1. app-এর নিজের bus connection-এ menubar ও action group export করে
     (/com/muxdesk/menu/<short>, /com/muxdesk/actions/<short>)
  2. Registrar-কে RegisterWindow ডাকে (আগে (u,s,o,o) — menubar path +
     actions path; না মিললে canonical (u,s,o) চেষ্টা)
  3. shutdown-এ UnregisterWindow + unexport

সবকিছু try/except-এ মোড়া: registrar/bus না থাকলে None ফেরে,
app আগের মতোই চলে — global menu শুধু বাড়তি।
gi না থাকলেও import হয় (window_key খাঁটি zlib) যাতে test চলতে পারে।
"""
from __future__ import annotations

import zlib

try:
    from gi.repository import Gio, GLib
except ImportError:  # headless test পরিবেশ
    Gio = GLib = None  # type: ignore[assignment]

REGISTRAR_NAME = "com.canonical.AppMenu.Registrar"
REGISTRAR_PATH = "/com/canonical/AppMenu/Registrar"
REGISTRAR_IFACE = "com.canonical.AppMenu.Registrar"


def window_key(app_id: str) -> int:
    """APP_ID-র stable 32-bit hash — একই app-এ সবসময় একই মান।"""
    return zlib.crc32(app_id.encode("utf-8")) & 0xFFFFFFFF


def menu_path(short: str) -> str:
    return f"/com/muxdesk/menu/{short}"


def actions_path(short: str) -> str:
    return f"/com/muxdesk/actions/{short}"


class GlobalMenuHandle:
    def __init__(self, conn, short: str, key: int, menu_id: int, action_id: int):
        self.conn = conn
        self.short = short
        self.key = key
        self.menu_id = menu_id
        self.action_id = action_id
        self.registered = False


def setup_global_menu(app, short: str, menubar, action_group) -> GlobalMenuHandle | None:
    """do_startup-এ ডাকো (bus পাওয়ার পর)। ব্যর্থ হলে None — app চলবে।"""
    if Gio is None:
        return None
    try:
        conn = app.get_dbus_connection()
        if conn is None:
            return None
        mpath, apath = menu_path(short), actions_path(short)
        menu_id = conn.export_menu_model(mpath, menubar)
        action_id = conn.export_action_group(apath, action_group)
        handle = GlobalMenuHandle(conn, short, window_key(app.get_application_id()),
                                  menu_id, action_id)
        service = conn.get_unique_name()
        # আগে spec-এর (u,s,o,o): menubar path + actions path
        try:
            conn.call_sync(REGISTRAR_NAME, REGISTRAR_PATH, REGISTRAR_IFACE,
                           "RegisterWindow",
                           GLib.Variant("(usoo)", (handle.key, service, mpath, apath)),
                           None, Gio.DBusCallFlags.NONE, 2000, None)
            handle.registered = True
        except Exception:
            # canonical আসল signature (u,s,o) দিয়ে আবার চেষ্টা
            try:
                conn.call_sync(REGISTRAR_NAME, REGISTRAR_PATH, REGISTRAR_IFACE,
                               "RegisterWindow",
                               GLib.Variant("(uso)", (handle.key, service, mpath)),
                               None, Gio.DBusCallFlags.NONE, 2000, None)
                handle.registered = True
            except Exception:
                pass  # registrar নেই — export থাকল, panel পরে নিজে টানতে পারবে
        return handle
    except Exception:
        return None


def teardown_global_menu(handle: GlobalMenuHandle | None) -> None:
    """do_shutdown-এ ডাকো।"""
    if handle is None or Gio is None:
        return
    try:
        if handle.registered:
            handle.conn.call_sync(REGISTRAR_NAME, REGISTRAR_PATH, REGISTRAR_IFACE,
                                  "UnregisterWindow",
                                  GLib.Variant("(u)", (handle.key,)),
                                  None, Gio.DBusCallFlags.NONE, 1000, None)
    except Exception:
        pass
    try:
        handle.conn.unexport_action_group(handle.action_id)
        handle.conn.unexport_menu_model(handle.menu_id)
    except Exception:
        pass
