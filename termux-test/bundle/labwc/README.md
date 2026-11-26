# labwc — MuxLike DE-র window manager সাজ

আমাদের নিজের DE-র ভিত্তি **wlroots + labwc**। এই ফোল্ডারের ফাইলগুলো
সরাসরি ব্যবহারযোগ্য — বসানোর জায়গা:

| ফাইল | বসবে |
|---|---|
| `rc.xml` | `~/.config/labwc/rc.xml` |
| `menu.xml` | `~/.config/labwc/menu.xml` |
| `autostart` | `~/.config/labwc/autostart` (executable) |
| `environment` | `~/.config/labwc/environment` |
| `panel/*` | `~/.config/labwc/panel/` |
| `../theme/MuxLike/` | `~/.local/share/themes/MuxLike/` |

## যা যা আছে

- **Keybind:** Super+Q বন্ধ, Super+M minimize, Super+F fullscreen,
  Super+Enter terminal (foot), Super+Space launcher (fuzzel),
  Super+E Files, Super+←/→/↑ snap/maximize, Super+1..4 desktop,
  Alt+Tab window বদল, volume key, Print screenshot (grim)।
- **Window নিয়ম:** dialog-ধাঁচের ছোট app (Nautilus dialog, pavucontrol)
  float; সব window-তে server-side decoration (আমাদের titlebar)।
- **Focus:** click-to-focus, নতুন window-তে focus — mouse-এর পেছনে
  ঘোরে না (ফোনের touch-এর জন্য এটাই ঠিক)।
- **Autostart:** swaybg wallpaper + waybar top bar + waybar dock +
  mako notification + polkit agent।

## দরকারি package (Arch)

`labwc waybar swaybg fuzzel foot mako grim wl-clipboard polkit-gnome`

## সৎ সীমা

- Runtime পরীক্ষা এই মেশিনে হয়নি (labwc binary নেই) — XML parse
  যাচাই হয়েছে মাত্র। Integration chat আসল পরিবেশে চালিয়ে দেখবে।
- waybar dock-এ macOS-এর মতো আসল magnification নেই; CSS hover-এ
  হালকা বড় হওয়া পর্যন্তই।
