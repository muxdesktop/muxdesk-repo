# Mux Settings — নিজের settings app (GTK4)

## সহজ কথায়
আগে Mux Settings ছিল gnome-control-center-এর launcher। এখন **নিজের app** —
নিজের code। gnome-control-center লাগে না।

## চালানো
```sh
# নির্ভরতা (Arch/MuxDesk): sudo pacman -S python python-gobject gtk4
./mux-settings         # dev tree থেকে সরাসরি
# বা বসিয়ে: ../install-own-apps.sh && mux-settings
```

## তৈরি ✅
- Sidebar (218px) + ৪ পেজ: About / Wi-Fi / Display / Appearance
- **About:** আমাদের logo (repo-র `assets/logo.svg` বা বসানো জায়গা থেকে),
  MuxDesk নাম, `/etc/os-release` থেকে NAME/VERSION/ID/ID_LIKE,
  hostname, kernel, arch, memory — সব সরাসরি পড়া, বানানো তথ্য না
  (পড়ার code: `../common/muxdesk_common.py`, test আছে)
- **Appearance (আসল control):** Dark mode switch — GTK সেটিং + gsettings
  `color-scheme` দুটোই বদলায়; Accent রঙ (violet/mint/coral) পছন্দ
  `~/.config/muxdesk/accent.conf`-এ সংরক্ষণ হয়
- **Wi-Fi (শুধু তথ্য):** `/proc/net/wireless` + nmcli (থাকলে) থেকে
  interface/signal/সক্রিয় connection দেখায়
- **Display (শুধু তথ্য):** Gdk থেকে monitor তালিকা — আকার/scale/refresh
- **Global menu (panel):** app menu (About → About পেজ, Quit) DBus-এ export
  (`/com/muxdesk/menu/settings` + `/com/muxdesk/actions/settings`),
  AppMenu Registrar-এ RegisterWindow; registrar না থাকলেও app চলে

## বাকি / সৎ সীমা ❌
- **Wi-Fi বদলানো যায় না** — rootless-এ Wi-Fi Android/host-এর হাতে।
  পেজে বড় banner-এ সেটা লেখা আছে; ভুয়া switch বসানো হয়নি।
- **Display বদলানো যায় না** — resolution/scale MuxDesktop app-এর
  display server-এর ব্যাপার; এই পেজ শুধু তথ্য দেখায়।
- Accent রঙ এখন শুধু conf ফাইলে লেখা হয় — shell/CSS এখনো ওই ফাইল
  পড়ে নিজে বদলায় না (shell অংশের কাজ বাকি)।
- Runtime test হয়নি — এই পরিবেশে GTK/gi নেই। যাচাই: py_compile +
  pure-helper test (os-release ও wireless parsing)।
- Sound/Bluetooth/Users ইত্যাদি পেজ নেই — পরে আসবে, প্রতিটা সৎ
  "বদলানো যায় / শুধু তথ্য" ভাগ মেনে।
