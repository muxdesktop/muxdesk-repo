"""Wi-Fi / display-এর খাঁটি-Python তথ্য পড়া — gi ছাড়া, তাই test করা যায়।

সৎ নীতি: rootless (proot, root নেই) পরিবেশে Wi-Fi বা display-এর
সেটিং বদলানো যায় না — Android/host-এর হাতে। এখানে শুধু যা পড়া
যায় তাই ফেরত আসে; কিছু না পেলে খালি/None, মিথ্যে তথ্য না।
"""
from __future__ import annotations

import shutil
import subprocess


def parse_proc_net_wireless(text: str) -> list[dict[str, str]]:
    """'/proc/net/wireless'-এর লেখা -> [{interface, link_quality, signal_dbm}]"""
    rows: list[dict[str, str]] = []
    for line in text.splitlines()[2:]:  # প্রথম ২ লাইন header
        if ":" not in line:
            continue
        iface, _, rest = line.partition(":")
        fields = rest.split()
        if len(fields) >= 3:
            rows.append({
                "interface": iface.strip(),
                "link_quality": fields[1].rstrip("."),
                "signal_dbm": fields[2].rstrip("."),
            })
    return rows


def wifi_interfaces() -> list[dict[str, str]]:
    try:
        with open("/proc/net/wireless", encoding="utf-8", errors="replace") as fh:
            return parse_proc_net_wireless(fh.read())
    except OSError:
        return []


def active_connections() -> list[str]:
    """nmcli থাকলে সক্রিয় connection-এর নাম; না থাকলে খালি তালিকা।"""
    if shutil.which("nmcli") is None:
        return []
    try:
        out = subprocess.run(
            ["nmcli", "-t", "-f", "NAME", "connection", "show", "--active"],
            capture_output=True, text=True, timeout=5, check=False)
        return [ln.strip() for ln in out.stdout.splitlines() if ln.strip()]
    except (OSError, subprocess.TimeoutExpired):
        return []


def has_network_manager() -> bool:
    return shutil.which("nmcli") is not None
