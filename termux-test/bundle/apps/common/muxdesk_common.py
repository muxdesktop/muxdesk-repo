"""MuxDesk common helpers — gi ছাড়া খাঁটি Python, তাই headless test চলে।

দুই app-ই (Mux Files / Mux Settings) এই module ব্যবহার করে:
os-release পড়া, logo খোঁজা, system তথ্য।
"""
from __future__ import annotations

import os
import platform
import shutil

# os-release-এর জায়গা: আগে env override (test-এর জন্য), তারপর আসল ফাইল,
# তারপর Termux/proot-এর ভিতরের সম্ভাব্য জায়গা।
OS_RELEASE_CANDIDATES = (
    "/etc/os-release",
    "/usr/lib/os-release",
)


def parse_os_release(text: str) -> dict[str, str]:
    """os-release ফাইলের লেখা -> dict। উদ্ধৃতি (quote) খুলে দেয়।"""
    data: dict[str, str] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        data[key.strip()] = value.replace('\\"', '"')
    return data


def read_os_release() -> dict[str, str]:
    """আসল os-release পড়ে। না পেলে খালি dict (মিথ্যে তথ্য বানানো হয় না)।"""
    override = os.environ.get("MUXDESK_OS_RELEASE")
    paths = (override,) if override else OS_RELEASE_CANDIDATES
    for path in paths:
        if path and os.path.isfile(path):
            try:
                with open(path, encoding="utf-8", errors="replace") as fh:
                    return parse_os_release(fh.read())
            except OSError:
                continue
    return {}


def find_logo() -> str | None:
    """আমাদের logo.svg/png-র প্রথম পাওয়া পথ, না পেলে None।

    খোঁজার ক্রম: env -> বসানো জায়গা -> এই repo-র assets (dev-এ চালালে)।
    """
    env = os.environ.get("MUXDESK_LOGO")
    if env and os.path.isfile(env):
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        "/usr/share/icons/hicolor/scalable/apps/muxdesk.svg",
        "/usr/share/muxdesk/logo.svg",
        os.path.expanduser("~/.local/share/muxdesk/logo.svg"),
        # repo layout: apps/common/ -> ../../assets/logo.svg
        os.path.join(here, "..", "..", "assets", "logo.svg"),
        os.path.join(here, "..", "..", "..", "muxdesk", "assets", "logo.svg"),
    ]
    for path in candidates:
        path = os.path.normpath(path)
        if os.path.isfile(path):
            return path
    return None


def system_info() -> dict[str, str]:
    """About পেজের তথ্য — যা সত্যি পাওয়া যায় শুধু তাই।"""
    rel = read_os_release()
    uname = platform.uname()
    info = {
        "name": rel.get("PRETTY_NAME") or rel.get("NAME") or "MuxDesk",
        "distro_name": rel.get("NAME", ""),
        "version": rel.get("VERSION") or rel.get("VERSION_ID", ""),
        "id": rel.get("ID", ""),
        "id_like": rel.get("ID_LIKE", ""),
        "hostname": uname.node or "",
        "kernel": f"{uname.system} {uname.release}" if uname.release else "",
        "arch": uname.machine or platform.machine() or "",
        "python": platform.python_version(),
    }
    try:
        with open("/proc/meminfo", encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("MemTotal:"):
                    kb = int(line.split()[1])
                    info["memory"] = f"{kb / 1024 / 1024:.1f} GB"
                    break
    except (OSError, ValueError, IndexError):
        pass
    return info


def disk_free_text(path: str) -> str:
    """'<free> free' ধরনের ছোট লেখা; না পারলে খালি।"""
    try:
        usage = shutil.disk_usage(path)
        return f"{usage.free / (1024 ** 3):.1f} GB free"
    except OSError:
        return ""


# MuxDesk palette — DESIGN.md অংশ ৩ক (FINAL)
PALETTE = {
    "violet": "#7c5cff",
    "violet_deep": "#5a3fd4",
    "mint": "#00c2a8",
    "coral": "#ff6f61",
    "ink": "#12141f",
    "paper": "#e8eaf6",
}
