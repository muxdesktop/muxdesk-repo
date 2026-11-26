"""Mux Files — Application entry. কেন Python/GTK4: নিচে README-তে যুক্তি।

Global menu: do_startup-এ menubar + unified action group বানিয়ে
DBus-এ export করা হয় (globalmenu.py) — panel সেটা টেনে দেখায়।
"""
from __future__ import annotations

import os
import sys

from gi.repository import GLib, Gio, Gtk

from . import APP_ID
from .globalmenu import setup_global_menu, teardown_global_menu
from .window import MuxFilesWindow

CSS_CANDIDATES = (
    os.path.join(os.path.dirname(__file__), "data", "mux-files.css"),
    "/usr/share/muxdesk/mux-files/mux-files.css",
    os.path.expanduser("~/.local/share/muxdesk/mux-files/mux-files.css"),
)


def _load_css():
    for path in CSS_CANDIDATES:
        if os.path.isfile(path):
            provider = Gtk.CssProvider()
            provider.load_from_path(path)
            from gi.repository import Gdk
            Gtk.StyleContext.add_provider_for_display(
                Gdk.Display.get_default(), provider,
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
            return


def build_menubar() -> Gio.Menu:
    """Panel-এ যাওয়া menubar। action নামগুলো exported group-এর
    plain নাম (globalmenu.py-র নোট দেখো)।"""
    menubar = Gio.Menu()

    file_menu = Gio.Menu()
    file_menu.append("New Folder", "new-folder")
    file_menu.append("Properties", "properties")
    section = Gio.Menu()
    section.append("Close Window", "close")
    file_menu.append_section(None, section)
    menubar.append_submenu("File", file_menu)

    edit_menu = Gio.Menu()
    edit_menu.append("Rename", "rename")
    edit_menu.append("Move to Trash", "trash")
    menubar.append_submenu("Edit", edit_menu)

    view_menu = Gio.Menu()
    view_menu.append("Grid View", "view-grid")
    view_menu.append("List View", "view-list")
    sort_menu = Gio.Menu()
    for label, key in (("Name", "name"), ("Size", "size"), ("Modified", "modified")):
        item = Gio.MenuItem.new(label, "sort")
        item.set_attribute_value("target", GLib.Variant.new_string(key))
        sort_menu.append_item(item)
    view_menu.append_submenu("Sort By", sort_menu)
    view_menu.append("Search", "search")
    menubar.append_submenu("View", view_menu)

    help_menu = Gio.Menu()
    help_menu.append("About Mux Files", "about")
    menubar.append_submenu("Help", help_menu)
    return menubar


class MuxFilesApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID,
                         flags=Gio.ApplicationFlags.HANDLES_OPEN)
        self._gm = None

    # --- global menu-র unified action group (window-তে delegate) ---
    def _win(self) -> MuxFilesWindow | None:
        win = self.props.active_window
        return win if isinstance(win, MuxFilesWindow) else None

    def _build_action_group(self) -> Gio.SimpleActionGroup:
        group = Gio.SimpleActionGroup()

        def add(name, fn):
            act = Gio.SimpleAction.new(name, None)
            act.connect("activate", lambda *_a, f=fn: f())
            group.add_action(act)

        def win_do(method):
            def _run():
                win = self._win()
                if win is not None:
                    getattr(win, method)()
            return _run

        add("new-folder", win_do("new_folder"))
        add("properties", win_do("show_properties"))
        add("rename", win_do("rename_selected"))
        add("trash", win_do("trash_selected"))
        add("search", win_do("focus_search"))
        add("view-grid", lambda: (w := self._win()) and w.set_view("grid"))
        add("view-list", lambda: (w := self._win()) and w.set_view("list"))
        add("close", lambda: (w := self._win()) and w.close())
        add("about", win_do("show_about"))

        sort = Gio.SimpleAction.new_stateful(
            "sort", GLib.VariantType.new("s"), GLib.Variant.new_string("name"))

        def _sort(action, value):
            action.set_state(value)
            win = self._win()
            if win is not None:
                win.set_sort(value.get_string())

        sort.connect("change-state", _sort)
        group.add_action(sort)
        return group

    def do_startup(self):
        Gtk.Application.do_startup(self)
        # traffic-light বান্ধব: window বোতাম বামে (close, minimize, maximize)
        settings = Gtk.Settings.get_default()
        if settings is not None:
            settings.set_property("gtk-decoration-layout", "close,minimize,maximize:")
        _load_css()
        # global menu export — registrar না থাকলেও app চলবে (helper-এ try/except)
        self._gm = setup_global_menu(self, "files", build_menubar(),
                                     self._build_action_group())

    def do_shutdown(self):
        teardown_global_menu(self._gm)
        self._gm = None
        Gtk.Application.do_shutdown(self)

    def do_activate(self):
        win = self.props.active_window or MuxFilesWindow(self)
        win.present()

    def do_open(self, files, _n, _hint):
        win = self.props.active_window or MuxFilesWindow(self)
        if files and files[0].query_file_type(0, None) == Gio.FileType.DIRECTORY:
            win.navigate(files[0], record=False)
        win.present()


def main(argv: list[str] | None = None) -> int:
    return MuxFilesApp().run(argv if argv is not None else sys.argv)
