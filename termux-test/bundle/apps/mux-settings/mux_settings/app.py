"""Mux Settings — Application entry।

Global menu: do_startup-এ app menu (About, Quit) export হয়
(globalmenu.py) — panel সেটা টেনে দেখায়।
"""
from __future__ import annotations

import os
import sys

from gi.repository import Gdk, Gio, Gtk

from . import APP_ID
from .globalmenu import setup_global_menu, teardown_global_menu
from .window import MuxSettingsWindow

CSS_CANDIDATES = (
    os.path.join(os.path.dirname(__file__), "data", "mux-settings.css"),
    "/usr/share/muxdesk/mux-settings/mux-settings.css",
    os.path.expanduser("~/.local/share/muxdesk/mux-settings/mux-settings.css"),
)


def build_menubar() -> Gio.Menu:
    menubar = Gio.Menu()
    app_menu = Gio.Menu()
    app_menu.append("About Mux Settings", "about")
    section = Gio.Menu()
    section.append("Quit", "quit")
    app_menu.append_section(None, section)
    menubar.append_submenu("Mux Settings", app_menu)
    return menubar


class MuxSettingsApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID, flags=Gio.ApplicationFlags.FLAGS_NONE)
        self._gm = None

    def _build_action_group(self) -> Gio.SimpleActionGroup:
        group = Gio.SimpleActionGroup()

        about = Gio.SimpleAction.new("about", None)

        def _about(*_args):
            win = self.props.active_window
            if isinstance(win, MuxSettingsWindow):
                win.show_page("about")

        about.connect("activate", _about)
        group.add_action(about)

        quit_act = Gio.SimpleAction.new("quit", None)
        quit_act.connect("activate", lambda *_a: self.quit())
        group.add_action(quit_act)
        return group

    def do_startup(self):
        Gtk.Application.do_startup(self)
        settings = Gtk.Settings.get_default()
        if settings is not None:  # traffic-light বান্ধব: বোতাম বামে
            settings.set_property("gtk-decoration-layout", "close,minimize,maximize:")
        for path in CSS_CANDIDATES:
            if os.path.isfile(path):
                provider = Gtk.CssProvider()
                provider.load_from_path(path)
                Gtk.StyleContext.add_provider_for_display(
                    Gdk.Display.get_default(), provider,
                    Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
                break
        self._gm = setup_global_menu(self, "settings", build_menubar(),
                                     self._build_action_group())

    def do_shutdown(self):
        teardown_global_menu(self._gm)
        self._gm = None
        Gtk.Application.do_shutdown(self)

    def do_activate(self):
        win = self.props.active_window or MuxSettingsWindow(self)
        win.present()


def main(argv: list[str] | None = None) -> int:
    return MuxSettingsApp().run(argv if argv is not None else sys.argv)
