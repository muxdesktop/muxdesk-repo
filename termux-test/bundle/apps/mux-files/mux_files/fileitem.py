"""একটা ফাইল/ফোল্ডারের তথ্য ধরে রাখা GObject — grid/list দুই view-ই এটা দেখায়।"""
from __future__ import annotations

import os
import time

from gi.repository import Gio, GObject


class FileItem(GObject.Object):
    __gtype_name__ = "MuxFileItem"

    name = GObject.Property(type=str, default="")
    path = GObject.Property(type=str, default="")
    is_dir = GObject.Property(type=bool, default=False)
    size = GObject.Property(type=int, default=0)
    modified = GObject.Property(type=int, default=0)  # unix time, 0 = অজানা

    def __init__(self, gfile: Gio.File, info: Gio.FileInfo | None = None):
        super().__init__()
        self.gfile = gfile
        self.path = gfile.get_path() or gfile.get_uri()
        if info is not None:
            self.name = info.get_display_name() or info.get_name() or ""
            self.is_dir = info.get_file_type() == Gio.FileType.DIRECTORY
            self.size = 0 if self.is_dir else int(info.get_size())
            mod = info.get_modification_date_time()
            self.modified = int(mod.to_unix()) if mod else 0
            self.icon = info.get_icon()
        else:
            self.name = gfile.get_basename() or self.path
            self.is_dir = os.path.isdir(self.path)
            self.icon = None
        if self.icon is None:
            kind = "folder" if self.is_dir else "text-x-generic"
            self.icon = Gio.ThemedIcon.new(kind)

    # --- প্রদর্শনের জন্য ছোট সহায়ক ---
    def size_text(self) -> str:
        if self.is_dir:
            return "—"
        size = float(self.size)
        for unit in ("B", "KB", "MB", "GB", "TB"):
            if size < 1024 or unit == "TB":
                return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
            size /= 1024
        return ""

    def modified_text(self) -> str:
        if not self.modified:
            return ""
        return time.strftime("%Y-%m-%d %H:%M", time.localtime(self.modified))

    def type_text(self) -> str:
        return "Folder" if self.is_dir else "File"
