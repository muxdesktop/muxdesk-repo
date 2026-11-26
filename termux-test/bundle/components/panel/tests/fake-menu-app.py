#!/usr/bin/env python3
"""fake-menu-app.py — global-menu contract-এর test app.

আমাদের আসল app গুলোর globalmenu.py যেভাবে করে, ঠিক সেভাবেই:
  1. নিজের bus connection-এ menubar export: /com/muxdesk/menu/fake
  2. action group export (plain নাম, prefix ছাড়া): /com/muxdesk/actions/fake
  3. Registrar-এ RegisterWindow — আগে (u,s,o,o), না হলে (u,s,o);
     window_key = crc32(APP_ID)

চালানো:
  dbus-run-session -- python3 tests/fake-menu-app.py --headless
      bus-only mode — window নেই, panel-এর D-Bus পথ test করার জন্য।
  python3 tests/fake-menu-app.py
      Gtk window খোলে (labwc-তে focused হলে app_id মিলবে) — main chat
      live screenshot test-এ এটাই ব্যবহার করবে।

প্রতিটা action activate হলে stdout-এ "ACTIVATED <name>" ছাপে।
"trash" action টা ইচ্ছে করে disabled — panel-এ ধূসর দেখানোর কথা।
"""
from __future__ import annotations

import sys
import zlib

from gi.repository import Gio, GLib

APP_ID = "com.muxdesk.fakemenu"
SHORT = "fake"
MENU_PATH = f"/com/muxdesk/menu/{SHORT}"
ACTIONS_PATH = f"/com/muxdesk/actions/{SHORT}"
REGISTRAR = "com.canonical.AppMenu.Registrar"
REGISTRAR_PATH = "/com/canonical/AppMenu/Registrar"


def window_key() -> int:
    return zlib.crc32(APP_ID.encode("utf-8")) & 0xFFFFFFFF


def build_menubar() -> Gio.Menu:
    top = Gio.Menu()

    file_menu = Gio.Menu()
    file_menu.append("New Window", "new-window")
    file_menu.append("Close", "close")
    top.append_submenu("File", file_menu)

    edit_menu = Gio.Menu()
    edit_menu.append("Rename", "rename")
    edit_menu.append("Move to Trash", "trash")  # disabled action
    top.append_submenu("Edit", edit_menu)

    view_menu = Gio.Menu()
    view_menu.append("Grid View", "view-grid")
    view_menu.append("List View", "view-list")
    top.append_submenu("View", view_menu)

    help_menu = Gio.Menu()
    help_menu.append("About Fake", "about")
    top.append_submenu("Help", help_menu)
    return top


def build_actions() -> Gio.SimpleActionGroup:
    group = Gio.SimpleActionGroup()
    for name in ("new-window", "close", "rename", "trash",
                 "view-grid", "view-list", "about"):
        act = Gio.SimpleAction.new(name, None)
        act.connect("activate", lambda a, _p, n=name: print(f"ACTIVATED {n}", flush=True))
        if name == "trash":
            act.set_enabled(False)
        group.add_action(act)
    return group


def register(conn) -> None:
    service = conn.get_unique_name()
    key = window_key()
    try:
        conn.call_sync(REGISTRAR, REGISTRAR_PATH, REGISTRAR,
                       "RegisterWindow",
                       GLib.Variant("(usoo)", (key, service, MENU_PATH, ACTIONS_PATH)),
                       None, Gio.DBusCallFlags.NONE, 2000, None)
        print(f"REGISTERED (usoo) key={key} service={service}", flush=True)
        return
    except Exception as e:
        print(f"(usoo) failed: {e.message} — trying (uso)", flush=True)
    try:
        conn.call_sync(REGISTRAR, REGISTRAR_PATH, REGISTRAR,
                       "RegisterWindow",
                       GLib.Variant("(uso)", (key, service, MENU_PATH)),
                       None, Gio.DBusCallFlags.NONE, 2000, None)
        print(f"REGISTERED (uso) key={key} service={service}", flush=True)
    except Exception as e:
        print(f"REGISTER FAILED: {e.message}", flush=True)


def main() -> int:
    headless = "--headless" in sys.argv
    app = Gio.Application(application_id=APP_ID,
                          flags=Gio.ApplicationFlags.FLAGS_NONE)
    state = {}

    def on_startup(a):
        conn = a.get_dbus_connection()
        state["menu_id"] = conn.export_menu_model(MENU_PATH, build_menubar())
        state["act_id"] = conn.export_action_group(ACTIONS_PATH, build_actions())
        register(conn)
        a.hold()

    def on_activate(a):
        if headless:
            return
        try:
            import gi
            gi.require_version("Gtk", "4.0")
            from gi.repository import Gtk
        except Exception:
            return
        win = Gtk.ApplicationWindow(application=a, title="Fake Menu App")
        win.set_default_size(420, 240)
        win.set_child(Gtk.Label(label="Fake app — panel-এ File/Edit/View/Help দেখুন"))
        win.present()
        state["win"] = win

    app.connect("startup", on_startup)
    app.connect("activate", on_activate)
    return app.run([])


if __name__ == "__main__":
    sys.exit(main())
