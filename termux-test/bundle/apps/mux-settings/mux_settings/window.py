"""Mux Settings-এর main window — sidebar + পেজ (About/Wi-Fi/Display/Appearance)।

সৎ নিয়ম: যেই পেজ rootless-এ আসলে বদলানো যায় না (Wi-Fi, Display),
সেখানে বড় করে "শুধু তথ্য" banner — ভুয়া switch বসানো হয়নি।
Appearance আসলেই বদলায় (per-user GTK/gsettings), তাই ওখানে আসল control।
"""
from __future__ import annotations

import os
import subprocess
import sys

from gi.repository import Gdk, Gio, Gtk

for _cand in (
    os.path.join(os.path.dirname(__file__), "..", "..", "common"),
    "/usr/share/muxdesk/common",
    os.path.expanduser("~/.local/share/muxdesk/common"),
):
    _cand = os.path.normpath(_cand)
    if os.path.isdir(_cand) and _cand not in sys.path:
        sys.path.insert(0, _cand)

from muxdesk_common import PALETTE, find_logo, system_info  # noqa: E402
from .sysinfo_extra import active_connections, has_network_manager, wifi_interfaces  # noqa: E402

PAGES = [
    ("about", "আমাদের সম্পর্কে (About)", "computer-symbolic"),
    ("wifi", "Wi-Fi", "network-wireless-symbolic"),
    ("display", "Display", "video-display-symbolic"),
    ("appearance", "চেহারা (Appearance)", "applications-graphics-symbolic"),
]


def _row(title: str, value: str) -> Gtk.Box:
    box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12,
                  margin_top=6, margin_bottom=6, margin_start=14, margin_end=14)
    box.append(Gtk.Label(label=title, xalign=0, hexpand=True))
    val = Gtk.Label(label=value or "—", xalign=1, selectable=True)
    val.add_css_class("dim-label")
    box.append(val)
    return box


def _info_banner(text: str) -> Gtk.Label:
    label = Gtk.Label(label=text, wrap=True, xalign=0)
    label.add_css_class("mux-info-banner")
    return label


def _page_box() -> Gtk.Box:
    return Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8,
                   margin_top=16, margin_bottom=16, margin_start=16, margin_end=16)


class MuxSettingsWindow(Gtk.ApplicationWindow):
    __gtype_name__ = "MuxSettingsWindow"

    def __init__(self, app: Gtk.Application):
        super().__init__(application=app, title="Mux Settings")
        self.set_default_size(860, 560)
        self.add_css_class("mux-settings-window")

        header = Gtk.HeaderBar()
        header.set_title_widget(Gtk.Label(label="Mux Settings"))
        self.set_titlebar(header)

        body = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        self.set_child(body)

        # --- sidebar ---
        side = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        side.add_css_class("mux-settings-sidebar")
        side.set_size_request(218, -1)
        self._list = Gtk.ListBox(selection_mode=Gtk.SelectionMode.SINGLE)
        self._list.add_css_class("mux-places")
        self._list.connect("row-activated", self._on_page)
        self._rows: dict[str, Gtk.ListBoxRow] = {}
        for key, label, icon in PAGES:
            row = Gtk.ListBoxRow()
            row.mux_page = key  # type: ignore[attr-defined]
            box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            box.append(Gtk.Image.new_from_icon_name(icon))
            box.append(Gtk.Label(label=label, xalign=0))
            row.set_child(box)
            self._list.append(row)
            self._rows[key] = row
        side.append(self._list)
        body.append(side)
        body.append(Gtk.Separator(orientation=Gtk.Orientation.VERTICAL))

        # --- পেজগুলো ---
        self._stack = Gtk.Stack(hexpand=True, vexpand=True)
        self._stack.add_named(Gtk.ScrolledWindow(child=self._about_page()), "about")
        self._stack.add_named(Gtk.ScrolledWindow(child=self._wifi_page()), "wifi")
        self._stack.add_named(Gtk.ScrolledWindow(child=self._display_page()), "display")
        self._stack.add_named(Gtk.ScrolledWindow(child=self._appearance_page()), "appearance")
        body.append(self._stack)
        self._list.select_row(self._rows["about"])

    def _on_page(self, _list, row):
        self._stack.set_visible_child_name(row.mux_page)

    def show_page(self, key: str):
        """Global menu / app action থেকে পেজ বদলানোর public method।"""
        if key in self._rows:
            self._list.select_row(self._rows[key])
            self._stack.set_visible_child_name(key)

    # ------------------------------------------------------------------
    # About — os-release থেকে আসল তথ্য + আমাদের logo
    # ------------------------------------------------------------------
    def _about_page(self) -> Gtk.Box:
        info = system_info()
        box = _page_box()

        logo_path = find_logo()
        if logo_path:
            picture = Gtk.Picture.new_for_filename(logo_path)
            picture.set_size_request(128, 128)
            picture.set_halign(Gtk.Align.CENTER)
            box.append(picture)
        else:
            box.append(Gtk.Label(label="(logo পাওয়া যায়নি)", halign=Gtk.Align.CENTER))

        name = Gtk.Label(label=info["distro_name"] or "MuxDesk",
                         halign=Gtk.Align.CENTER)
        name.add_css_class("mux-about-name")
        box.append(name)
        sub = Gtk.Label(label=info["name"], halign=Gtk.Align.CENTER)
        sub.add_css_class("dim-label")
        box.append(sub)

        frame = Gtk.Frame()
        rows = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        for title, key in (("নাম", "name"), ("Version", "version"),
                           ("Distro ID", "id"), ("ভিত্তি (ID_LIKE)", "id_like"),
                           ("Hostname", "hostname"), ("Kernel", "kernel"),
                           ("Architecture", "arch"), ("Memory", "memory")):
            if info.get(key):
                rows.append(_row(title, info[key]))
        frame.set_child(rows)
        box.append(frame)
        box.append(_info_banner(
            "এই তথ্য /etc/os-release ও system থেকে সরাসরি পড়া — "
            "MuxDesk (ID=muxdesk, ID_LIKE=arch) আমাদের Arch-ভিত্তিক distro।"))
        return box

    # ------------------------------------------------------------------
    # Wi-Fi — শুধু তথ্য (rootless-এ বদলানো যায় না)
    # ------------------------------------------------------------------
    def _wifi_page(self) -> Gtk.Box:
        box = _page_box()
        box.append(_info_banner(
            "শুধু তথ্য — এই পেজ থেকে Wi-Fi বদলানো যায় না। MuxDesk root "
            "ছাড়া (rootless) চলে; Wi-Fi Android/host-এর হাতে, বদলাতে হলে "
            "ফোনের Settings থেকে বদলাতে হবে। ভুয়া switch এখানে বসানো হয়নি।"))
        ifaces = wifi_interfaces()
        conns = active_connections()
        frame = Gtk.Frame()
        rows = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        rows.append(_row("NetworkManager (nmcli)", "আছে" if has_network_manager() else "নেই"))
        if ifaces:
            for iface in ifaces:
                rows.append(_row(f"Interface {iface['interface']}",
                                 f"link {iface['link_quality']}, signal {iface['signal_dbm']} dBm"))
        else:
            rows.append(_row("Wireless interface", "পাওয়া যায়নি (/proc/net/wireless খালি)"))
        if conns:
            for conn in conns:
                rows.append(_row("সক্রিয় connection", conn))
        frame.set_child(rows)
        box.append(frame)
        return box

    # ------------------------------------------------------------------
    # Display — শুধু তথ্য (rootless-এ বদলানো যায় না)
    # ------------------------------------------------------------------
    def _display_page(self) -> Gtk.Box:
        box = _page_box()
        box.append(_info_banner(
            "শুধু তথ্য — resolution/scale এই পেজ থেকে বদলানো যায় না। "
            "Display server MuxDesktop app-এর ভিতরে; আকার বদলাতে app-এর "
            "orientation/size সেটিং লাগে (Display Server অংশের কাজ)।"))
        frame = Gtk.Frame()
        rows = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        display = Gdk.Display.get_default()
        if display is not None:
            monitors = display.get_monitors()
            for i in range(monitors.get_n_items()):
                mon = monitors.get_item(i)
                geo = mon.get_geometry()
                rows.append(_row(
                    f"Monitor {i + 1}",
                    f"{geo.width}×{geo.height} · scale {mon.get_scale_factor()} · "
                    f"{mon.get_refresh_rate() / 1000:.0f} Hz"))
            if monitors.get_n_items() == 0:
                rows.append(_row("Monitor", "তালিকা খালি"))
        else:
            rows.append(_row("Display", "পাওয়া যায়নি"))
        frame.set_child(rows)
        box.append(frame)
        return box

    # ------------------------------------------------------------------
    # Appearance — আসল control (per-user, rootless-এই কাজ করে)
    # ------------------------------------------------------------------
    def _appearance_page(self) -> Gtk.Box:
        box = _page_box()
        box.append(_info_banner(
            "এই সেটিংগুলো আসলেই বদলায় — per-user GTK সেটিং, root লাগে না। "
            "Dark mode পুরো session-এর GTK app-এ লাগে (gsettings থাকলে)।"))

        frame = Gtk.Frame()
        rows = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)

        dark_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12,
                           margin_top=8, margin_bottom=8, margin_start=14, margin_end=14)
        dark_row.append(Gtk.Label(label="Dark mode", xalign=0, hexpand=True))
        self._dark_switch = Gtk.Switch()
        settings = Gtk.Settings.get_default()
        if settings is not None:
            self._dark_switch.set_active(
                bool(settings.get_property("gtk-application-prefer-dark-theme")))
        self._dark_switch.connect("notify::active", self._on_dark_changed)
        dark_row.append(self._dark_switch)
        rows.append(dark_row)

        accent_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8,
                             margin_top=8, margin_bottom=8, margin_start=14, margin_end=14)
        accent_row.append(Gtk.Label(label="Accent রঙ", xalign=0, hexpand=True))
        self._accent_status = Gtk.Label(label="", xalign=1)
        for name in ("violet", "mint", "coral"):
            btn = Gtk.Button(label=name.capitalize())
            btn.connect("clicked", self._on_accent, name)
            accent_row.append(btn)
        rows.append(accent_row)
        rows.append(self._accent_status)
        frame.set_child(rows)
        box.append(frame)
        return box

    def _on_dark_changed(self, switch, _param):
        dark = switch.get_active()
        settings = Gtk.Settings.get_default()
        if settings is not None:
            settings.set_property("gtk-application-prefer-dark-theme", dark)
        # session জুড়ে লাগাতে gsettings (থাকলে) — না থাকলে শুধু এই app-এ
        try:
            subprocess.run(
                ["gsettings", "set", "org.gnome.desktop.interface", "color-scheme",
                 "prefer-dark" if dark else "prefer-light"],
                timeout=5, check=False, capture_output=True)
        except OSError:
            pass

    def _on_accent(self, _btn, name: str):
        """Accent পছন্দ ~/.config/muxdesk/accent.conf-এ লেখে — shell/CSS
        ওই ফাইল পড়ে রঙ নেয়। পুরো theme engine এখনো বানানো হয়নি (সৎ)।"""
        conf_dir = os.path.expanduser("~/.config/muxdesk")
        try:
            os.makedirs(conf_dir, exist_ok=True)
            with open(os.path.join(conf_dir, "accent.conf"), "w", encoding="utf-8") as fh:
                fh.write(f"accent={name}\ncolor={PALETTE[name]}\n")
            self._accent_status.set_text(
                f"Accent: {name} ({PALETTE[name]}) — সংরক্ষণ হয়েছে; "
                "shell পুনরায় চালু হলে পুরোপুরি লাগবে।")
        except OSError as err:
            self._accent_status.set_text(f"সংরক্ষণ হয়নি: {err}")
