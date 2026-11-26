"""Mux Files-এর main window.

নকশা: apps/MUX-FILES-DESIGN.md (অনুমোদিত polished look) —
sidebar 218px / row 32px, unified headerbar 44px, slim statusbar 26px,
নিচে path bar, frosted-glass আভা (CSS-তে alpha background, আসল blur
compositor-এর কাজ — সৎ নোট README-তে)।
"""
from __future__ import annotations

import os
import sys

from gi.repository import Gdk, Gio, GLib, Gtk, Pango

# common module (apps/common/) — dev layout ও বসানো layout দুটোতেই খোঁজে
for _cand in (
    os.path.join(os.path.dirname(__file__), "..", "..", "common"),
    "/usr/share/muxdesk/common",
    os.path.expanduser("~/.local/share/muxdesk/common"),
):
    _cand = os.path.normpath(_cand)
    if os.path.isdir(_cand) and _cand not in sys.path:
        sys.path.insert(0, _cand)
try:
    from muxdesk_common import disk_free_text
except ImportError:  # common না পেলেও app চলবে
    def disk_free_text(_path: str) -> str:  # type: ignore[misc]
        return ""

from .fileitem import FileItem

ATTRS = (
    "standard::name,standard::display-name,standard::type,"
    "standard::size,standard::icon,time::modified"
)

# sidebar: (লেবেল, icon, জায়গা) — জায়গা পরে _place_file() ঠিক করে
PLACES = [
    ("Home", "user-home-symbolic", "home"),
    ("Documents", "folder-documents-symbolic", "documents"),
    ("Downloads", "folder-download-symbolic", "downloads"),
    ("Pictures", "folder-pictures-symbolic", "pictures"),
    ("Music", "folder-music-symbolic", "music"),
    ("Videos", "folder-videos-symbolic", "videos"),
    ("Trash", "user-trash-symbolic", "trash"),
]


def _place_file(kind: str) -> Gio.File:
    special = {
        "documents": GLib.UserDirectory.DIRECTORY_DOCUMENTS,
        "downloads": GLib.UserDirectory.DIRECTORY_DOWNLOAD,
        "pictures": GLib.UserDirectory.DIRECTORY_PICTURES,
        "music": GLib.UserDirectory.DIRECTORY_MUSIC,
        "videos": GLib.UserDirectory.DIRECTORY_VIDEOS,
    }
    if kind == "home":
        return Gio.File.new_for_path(GLib.get_home_dir())
    if kind == "trash":
        return Gio.File.new_for_uri("trash:///")
    path = GLib.get_user_special_dir(special[kind])
    return Gio.File.new_for_path(path or GLib.get_home_dir())


class MuxFilesWindow(Gtk.ApplicationWindow):
    __gtype_name__ = "MuxFilesWindow"

    def __init__(self, app: Gtk.Application):
        super().__init__(application=app, title="Mux Files")
        self.set_default_size(980, 620)
        self.add_css_class("mux-files-window")

        self._current: Gio.File = _place_file("home")
        self._back: list[Gio.File] = []
        self._forward: list[Gio.File] = []
        self._sort_key = "name"

        # --- model শৃঙ্খল: store -> filter (search) -> sort -> selection ---
        self._store = Gio.ListStore(item_type=FileItem)
        self._filter = Gtk.CustomFilter.new(self._match_search)
        self._filtered = Gtk.FilterListModel.new(self._store, self._filter)
        self._sorter = Gtk.CustomSorter.new(self._compare)
        self._sorted = Gtk.SortListModel.new(self._filtered, self._sorter)
        self._selection = Gtk.SingleSelection.new(self._sorted)

        self._build_headerbar()
        self._build_body()
        self._build_actions()
        self.navigate(self._current, record=False)

    # ------------------------------------------------------------------
    # গঠন
    # ------------------------------------------------------------------
    def _build_headerbar(self):
        header = Gtk.HeaderBar()
        header.add_css_class("mux-headerbar")
        self.set_titlebar(header)

        # back / forward — একটা যুগ্ম pill
        nav = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        nav.add_css_class("linked")
        self._btn_back = Gtk.Button(icon_name="go-previous-symbolic")
        self._btn_back.set_tooltip_text("পেছনে")
        self._btn_back.connect("clicked", lambda *_: self.go_back())
        self._btn_fwd = Gtk.Button(icon_name="go-next-symbolic")
        self._btn_fwd.set_tooltip_text("সামনে")
        self._btn_fwd.connect("clicked", lambda *_: self.go_forward())
        nav.append(self._btn_back)
        nav.append(self._btn_fwd)
        header.pack_start(nav)

        btn_side = Gtk.Button(icon_name="view-sidebar-start-symbolic")
        btn_side.set_tooltip_text("Sidebar দেখাও/লুকাও")
        btn_side.connect("clicked", self._toggle_sidebar)
        header.pack_start(btn_side)

        # মাঝে breadcrumb (segmented pill)
        self._crumbs = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        self._crumbs.add_css_class("linked")
        self._crumbs.add_css_class("mux-breadcrumb")
        header.set_title_widget(self._crumbs)

        # ডানে: search, view toggle, sort menu, new folder
        self._search_entry = Gtk.SearchEntry(placeholder_text="খোঁজো…")
        self._search_entry.set_visible(False)
        self._search_entry.connect("search-changed", self._on_search_changed)
        self._search_entry.connect("stop-search", self._hide_search)
        header.pack_end(self._search_entry)

        btn_search = Gtk.ToggleButton(icon_name="system-search-symbolic")
        btn_search.set_tooltip_text("খোঁজো (Ctrl+F)")
        btn_search.connect("toggled", self._on_search_toggled)
        self._btn_search = btn_search
        header.pack_end(btn_search)

        views = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        views.add_css_class("linked")
        self._btn_grid = Gtk.ToggleButton(icon_name="view-grid-symbolic", active=True)
        self._btn_grid.set_tooltip_text("Grid view")
        self._btn_grid.connect("toggled", self._on_view_toggled, "grid")
        self._btn_list = Gtk.ToggleButton(icon_name="view-list-symbolic")
        self._btn_list.set_tooltip_text("List view")
        self._btn_list.connect("toggled", self._on_view_toggled, "list")
        views.append(self._btn_grid)
        views.append(self._btn_list)
        header.pack_end(views)

        sort_menu = Gio.Menu()
        for label, key in (("নাম অনুযায়ী", "name"), ("আকার অনুযায়ী", "size"),
                           ("তারিখ অনুযায়ী", "modified")):
            sort_menu.append(label, f"win.sort::{key}")
        btn_sort = Gtk.MenuButton(icon_name="view-sort-descending-symbolic",
                                  menu_model=sort_menu)
        btn_sort.set_tooltip_text("সাজাও")
        header.pack_end(btn_sort)

        btn_new = Gtk.Button(icon_name="folder-new-symbolic")
        btn_new.set_tooltip_text("নতুন folder")
        btn_new.connect("clicked", lambda *_: self.new_folder())
        header.pack_end(btn_new)

    def _build_body(self):
        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.set_child(root)

        paned = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        paned.set_vexpand(True)
        root.append(paned)

        # --- sidebar (218px, refined) ---
        self._sidebar_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self._sidebar_box.add_css_class("mux-sidebar")
        self._sidebar_box.set_size_request(218, -1)
        side_scroll = Gtk.ScrolledWindow()
        side_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        side_scroll.set_child(self._sidebar_box)
        paned.append(side_scroll)
        self._sidebar_scroll = side_scroll

        title = Gtk.Label(label="FAVORITES", xalign=0)
        title.add_css_class("mux-sidebar-title")
        self._sidebar_box.append(title)

        self._places_list = Gtk.ListBox(selection_mode=Gtk.SelectionMode.SINGLE)
        self._places_list.add_css_class("mux-places")
        self._places_list.connect("row-activated", self._on_place_activated)
        self._place_rows: dict[str, Gtk.ListBoxRow] = {}
        for label, icon, kind in PLACES:
            row = Gtk.ListBoxRow()
            row.mux_place = kind  # type: ignore[attr-defined]
            box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            box.append(Gtk.Image.new_from_icon_name(icon))
            box.append(Gtk.Label(label=label, xalign=0))
            row.set_child(box)
            self._places_list.append(row)
            self._place_rows[kind] = row
        self._sidebar_box.append(self._places_list)

        paned.append(Gtk.Separator(orientation=Gtk.Orientation.VERTICAL))

        # --- content: grid / list stack ---
        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        content.set_hexpand(True)
        paned.append(content)

        self._stack = Gtk.Stack()
        self._stack.set_vexpand(True)
        content.append(self._stack)

        self._grid = Gtk.GridView(model=self._selection, max_columns=8)
        self._grid.add_css_class("mux-grid")
        factory = Gtk.SignalListItemFactory()
        factory.connect("setup", self._grid_setup)
        factory.connect("bind", self._grid_bind)
        self._grid.set_factory(factory)
        self._grid.connect("activate", self._on_activate)
        grid_scroll = Gtk.ScrolledWindow(child=self._grid)
        grid_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self._stack.add_named(grid_scroll, "grid")

        self._column_view = self._make_column_view()
        list_scroll = Gtk.ScrolledWindow(child=self._column_view)
        list_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self._stack.add_named(list_scroll, "list")
        self._stack.set_visible_child_name("grid")

        self._empty = Gtk.Label(label="Folder is empty", visible=False)
        self._empty.add_css_class("mux-empty")
        content.append(self._empty)

        # --- নিচে: status + path bar (slim) ---
        bottom = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        bottom.add_css_class("mux-bottombar")
        self._status = Gtk.Label(label="", xalign=0)
        self._status.add_css_class("mux-status")
        bottom.append(self._status)
        self._path_entry = Gtk.Entry(hexpand=True)
        self._path_entry.add_css_class("mux-pathbar")
        self._path_entry.set_tooltip_text("Path — লিখে Enter চাপলে সেখানে যাবে (Ctrl+L)")
        self._path_entry.connect("activate", self._on_path_entered)
        bottom.append(self._path_entry)
        root.append(bottom)

        # ডান-click context menu (Open / Rename / Trash / Properties)
        menu = Gio.Menu()
        menu.append("খোলো", "win.open")
        menu.append("নাম বদলাও", "win.rename")
        menu.append("Trash-এ ফেলো", "win.trash")
        menu.append("তথ্য", "win.properties")
        self._context = Gtk.PopoverMenu.new_from_model(menu)
        self._context.set_parent(self)
        for view in (self._grid, self._column_view):
            click = Gtk.GestureClick(button=3)
            click.connect("pressed", self._on_right_click)
            view.add_controller(click)

        keys = Gtk.EventControllerKey()
        keys.connect("key-pressed", self._on_key)
        self.add_controller(keys)

    def _make_column_view(self) -> Gtk.ColumnView:
        view = Gtk.ColumnView(model=self._selection)
        view.add_css_class("mux-list")
        view.connect("activate", self._on_activate)

        def col(title, key, width):
            factory = Gtk.SignalListItemFactory()
            factory.connect("setup", lambda _f, item: item.set_child(Gtk.Label(xalign=0)))
            factory.connect("bind", lambda _f, item, k=key: item.get_child().set_text(
                getattr(item.get_item(), k)() if callable(getattr(item.get_item(), k, None))
                else str(getattr(item.get_item(), k, ""))))
            column = Gtk.ColumnViewColumn(title=title, factory=factory)
            column.set_fixed_width(width)
            return column

        name_col = Gtk.ColumnViewColumn(title="নাম")
        nf = Gtk.SignalListItemFactory()
        nf.connect("setup", self._name_setup)
        nf.connect("bind", self._name_bind)
        name_col.set_factory(nf)
        name_col.set_expand(True)
        view.append_column(name_col)
        view.append_column(col("আকার", "size_text", 100))
        view.append_column(col("ধরন", "type_text", 90))
        view.append_column(col("বদলানো", "modified_text", 150))
        return view

    # ------------------------------------------------------------------
    # actions / keyboard
    # ------------------------------------------------------------------
    def _build_actions(self):
        group = Gio.SimpleActionGroup()
        sort_action = Gio.SimpleAction.new_stateful(
            "sort", GLib.VariantType.new("s"), GLib.Variant.new_string("name"))
        sort_action.connect("change-state", self._on_sort_changed)
        group.add_action(sort_action)
        for name, fn in (("open", lambda *_: self._activate_selected()),
                         ("rename", lambda *_: self.rename_selected()),
                         ("trash", lambda *_: self.trash_selected()),
                         ("properties", lambda *_: self.show_properties())):
            act = Gio.SimpleAction.new(name, None)
            act.connect("activate", fn)
            group.add_action(act)
        self.insert_action_group("win", group)

    def _on_key(self, _ctrl, keyval, _code, state):
        ctrl = state & Gdk.ModifierType.CONTROL_MASK
        if ctrl and keyval in (Gdk.KEY_f, Gdk.KEY_F):
            self._btn_search.set_active(True)
            return True
        if ctrl and keyval in (Gdk.KEY_l, Gdk.KEY_L):
            self._path_entry.grab_focus()
            self._path_entry.select_region(0, -1)
            return True
        if keyval == Gdk.KEY_Delete:
            self.trash_selected()
            return True
        if keyval == Gdk.KEY_F2:
            self.rename_selected()
            return True
        if keyval == Gdk.KEY_F9:
            self._toggle_sidebar()
            return True
        return False

    # ------------------------------------------------------------------
    # navigation / loading
    # ------------------------------------------------------------------
    def navigate(self, gfile: Gio.File, record: bool = True):
        if record and self._current and gfile.get_uri() != self._current.get_uri():
            self._back.append(self._current)
            self._forward.clear()
        self._current = gfile
        self._load()
        self._update_chrome()

    def go_back(self):
        if self._back:
            self._forward.append(self._current)
            self._current = self._back.pop()
            self._load()
            self._update_chrome()

    def go_forward(self):
        if self._forward:
            self._back.append(self._current)
            self._current = self._forward.pop()
            self._load()
            self._update_chrome()

    def _load(self):
        """বর্তমান folder-এর জিনিস store-এ তোলে। বড় folder-এ sync পড়া —
        ফোনের স্থানীয় folder-এর জন্য যথেষ্ট; সৎ সীমা README-তে।"""
        self._store.remove_all()
        count = 0
        try:
            enum = self._current.enumerate_children(ATTRS, Gio.FileQueryInfoFlags.NONE, None)
            while True:
                info = enum.next_file(None)
                if info is None:
                    break
                child = self._current.get_child(info.get_name())
                self._store.append(FileItem(child, info))
                count += 1
            enum.close(None)
        except GLib.Error as err:
            self._status.set_text(f"খোলা গেল না: {err.message}")
            self._empty.set_visible(True)
            self._empty.set_text("এই জায়গাটা খোলা গেল না")
            return
        self._empty.set_visible(count == 0)
        free = disk_free_text(self._current.get_path() or "/")
        self._status.set_text(f"{count} items" + (f" · {free}" if free else ""))

    def _update_chrome(self):
        path = self._current.get_path()
        shown = path or self._current.get_uri()
        self._path_entry.set_text(shown)
        self.set_title(f"Mux Files — {self._current.get_basename() or shown}")
        self._btn_back.set_sensitive(bool(self._back))
        self._btn_fwd.set_sensitive(bool(self._forward))
        # breadcrumb পুনর্নির্মাণ
        child = self._crumbs.get_first_child()
        while child is not None:
            nxt = child.get_next_sibling()
            self._crumbs.remove(child)
            child = nxt
        if path:
            parts = [p for p in path.split(os.sep) if p]
            acc = os.sep
            btn = Gtk.Button(label="⌂")
            btn.connect("clicked", lambda *_: self.navigate(Gio.File.new_for_path("/")))
            self._crumbs.append(btn)
            for part in parts:
                acc = os.path.join(acc, part)
                target = acc
                btn = Gtk.Button(label=part)
                btn.connect("clicked", lambda *_a, t=target: self.navigate(Gio.File.new_for_path(t)))
                self._crumbs.append(btn)
        # sidebar selection মেলানো
        for kind, row in self._place_rows.items():
            if _place_file(kind).get_uri() == self._current.get_uri():
                self._places_list.select_row(row)

    # ------------------------------------------------------------------
    # view factories
    # ------------------------------------------------------------------
    def _grid_setup(self, _factory, item):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        box.set_size_request(96, 104)
        image = Gtk.Image(pixel_size=48)
        label = Gtk.Label(wrap=True, justify=Gtk.Justification.CENTER, lines=2,
                          ellipsize=Pango.EllipsizeMode.END)
        box.append(image)
        box.append(label)
        item.set_child(box)

    def _grid_bind(self, _factory, item):
        obj: FileItem = item.get_item()
        box = item.get_child()
        box.get_first_child().set_from_gicon(obj.icon)
        box.get_last_child().set_text(obj.name)

    def _name_setup(self, _factory, item):
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        box.append(Gtk.Image(pixel_size=20))
        box.append(Gtk.Label(xalign=0))
        item.set_child(box)

    def _name_bind(self, _factory, item):
        obj: FileItem = item.get_item()
        box = item.get_child()
        box.get_first_child().set_from_gicon(obj.icon)
        box.get_last_child().set_text(obj.name)

    # ------------------------------------------------------------------
    # ঘটনা (events)
    # ------------------------------------------------------------------
    def _on_place_activated(self, _list, row):
        self.navigate(_place_file(row.mux_place))

    def _on_activate(self, _view, position):
        item = self._selection.get_item(position)
        if item is not None:
            self._open_item(item)

    def _activate_selected(self):
        pos = self._selection.get_selected()
        if pos != Gtk.INVALID_LIST_POSITION:
            item = self._selection.get_item(pos)
            if item is not None:
                self._open_item(item)

    def _open_item(self, item: FileItem):
        if item.is_dir:
            self.navigate(item.gfile)
        else:
            Gio.AppInfo.launch_default_for_uri(item.gfile.get_uri(), None)

    def _toggle_sidebar(self, *_args):
        self._sidebar_scroll.set_visible(not self._sidebar_scroll.get_visible())

    def _on_search_toggled(self, btn):
        self._search_entry.set_visible(btn.get_active())
        if btn.get_active():
            self._search_entry.grab_focus()
        else:
            self._search_entry.set_text("")

    def _hide_search(self, *_args):
        self._btn_search.set_active(False)

    def _on_search_changed(self, entry):
        self._search_text = entry.get_text().lower()
        self._filter.changed(Gtk.FilterChange.DIFFERENT)

    _search_text = ""

    def _match_search(self, item: FileItem) -> bool:
        return not self._search_text or self._search_text in item.name.lower()

    def _on_view_toggled(self, btn, which):
        if not btn.get_active():
            return
        other = self._btn_list if which == "grid" else self._btn_grid
        other.set_active(False)
        self._stack.set_visible_child_name(which)

    def _on_sort_changed(self, action, value):
        action.set_state(value)
        self._sort_key = value.get_string()
        self._sorter.changed(Gtk.SorterChange.DIFFERENT)

    def _compare(self, a: FileItem, b: FileItem, _user_data=None) -> int:
        if a.is_dir != b.is_dir:  # folder আগে
            return -1 if a.is_dir else 1
        if self._sort_key == "size":
            return (a.size > b.size) - (a.size < b.size)
        if self._sort_key == "modified":
            return (b.modified > a.modified) - (b.modified < a.modified)
        return (a.name.lower() > b.name.lower()) - (a.name.lower() < b.name.lower())

    def _on_path_entered(self, entry):
        text = entry.get_text().strip()
        if text:
            target = (Gio.File.new_for_path(os.path.expanduser(text))
                      if not "://" in text else Gio.File.new_for_uri(text))
            if target.query_exists(None):
                self.navigate(target)
            else:
                self._status.set_text("ওই path পাওয়া গেল না")

    def _on_right_click(self, _gesture, _n, x, y):
        rect = Gdk.Rectangle()
        rect.x, rect.y, rect.width, rect.height = int(x), int(y), 1, 1
        self._context.set_pointing_to(rect)
        self._context.popup()

    # ------------------------------------------------------------------
    # Global menu / app action থেকে ডাকার public method
    # ------------------------------------------------------------------
    def set_view(self, which: str):
        btn = self._btn_grid if which == "grid" else self._btn_list
        btn.set_active(True)  # toggled handler-ই stack বদলায়

    def set_sort(self, key: str):
        if key in ("name", "size", "modified"):
            self._sort_key = key
            self._sorter.changed(Gtk.SorterChange.DIFFERENT)

    def focus_search(self):
        if not self._btn_search.get_active():
            self._btn_search.set_active(True)
        else:
            self._search_entry.grab_focus()

    def show_about(self):
        dialog = Gtk.AlertDialog(
            message="Mux Files",
            detail="MuxDesk-এর নিজের file manager (GTK4)।\nMuxLike DE-র অংশ — global menu সমর্থনসহ।",
            buttons=["ঠিক আছে"])
        dialog.show(self)

    # ------------------------------------------------------------------
    # ফাইল অপারেশন
    # ------------------------------------------------------------------
    def _selected_item(self) -> FileItem | None:
        pos = self._selection.get_selected()
        if pos == Gtk.INVALID_LIST_POSITION:
            return None
        return self._selection.get_item(pos)

    def _ask_text(self, title: str, initial: str, callback):
        dialog = Gtk.Window(transient_for=self, modal=True, title=title)
        dialog.set_default_size(340, -1)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10,
                      margin_top=14, margin_bottom=14, margin_start=14, margin_end=14)
        entry = Gtk.Entry(text=initial)
        entry.select_region(0, -1)
        box.append(entry)
        row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8,
                      halign=Gtk.Align.END)
        cancel = Gtk.Button(label="বাদ দাও")
        cancel.connect("clicked", lambda *_: dialog.close())
        ok = Gtk.Button(label="ঠিক আছে")
        ok.add_css_class("suggested-action")

        def _done(*_args):
            text = entry.get_text().strip()
            dialog.close()
            if text:
                callback(text)

        ok.connect("clicked", _done)
        entry.connect("activate", _done)
        row.append(cancel)
        row.append(ok)
        box.append(row)
        dialog.set_child(box)
        dialog.present()

    def new_folder(self):
        def _create(name):
            try:
                self._current.get_child(name).make_directory(None)
                self._load()
            except GLib.Error as err:
                self._status.set_text(f"Folder বানানো গেল না: {err.message}")
        self._ask_text("নতুন folder", "Untitled Folder", _create)

    def rename_selected(self):
        item = self._selected_item()
        if item is None:
            return

        def _rename(new_name):
            try:
                item.gfile.set_display_name(new_name, None)
                self._load()
            except GLib.Error as err:
                self._status.set_text(f"নাম বদলানো গেল না: {err.message}")
        self._ask_text("নাম বদলাও", item.name, _rename)

    def trash_selected(self):
        item = self._selected_item()
        if item is None:
            return
        try:
            item.gfile.trash(None)
            self._load()
        except GLib.Error as err:
            self._status.set_text(f"Trash-এ ফেলা গেল না: {err.message}")

    def show_properties(self):
        item = self._selected_item()
        if item is None:
            return
        text = (f"নাম: {item.name}\nধরন: {item.type_text()}\n"
                f"আকার: {item.size_text()}\nবদলানো: {item.modified_text()}\n"
                f"জায়গা: {item.path}")
        dialog = Gtk.AlertDialog(message="তথ্য", detail=text, buttons=["ঠিক আছে"])
        dialog.show(self)
