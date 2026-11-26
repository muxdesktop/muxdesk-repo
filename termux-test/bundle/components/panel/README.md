# mux-panel — MuxLike DE-র উপরের bar

সহজ কথায়: এটা আমাদের **নিজের লেখা** top bar — কোনো GNOME/extension না।
নিচে labwc (wlroots) compositor, উপরে এই panel `wlr-layer-shell`
প্রোটোকলে বসে। বামে Mux logo + চলমান app-এর নাম, ডানে network,
battery আর clock।

## কেন C + layer-shell, GTK না?

- labwc/wlroots জগতের নিজের ভাষা C আর নিজের প্রোটোকল layer-shell —
  GTK বসালে সেটা এই প্রোটোকলের উপরেই আরেকটা ভারী স্তর হতো।
- ফোনে GPU নেই, software rendering — সরাসরি `wl_shm` + cairo দিয়ে
  আঁকলে panel ছোট, দ্রুত ওঠে, RAM কম খায়। Dependency মাত্র:
  wayland-client, cairo, pango, glib — সবই Arch-এ হালকা প্যাকেজ।
- আর এই environment-এ ঠিক এই library গুলোই আছে বলে code টা এখানে
  **সত্যিই compile করে যাচাই করা গেছে** (GTK4 এখানে নেই-ই)।

## কী কী আছে

| অংশ | ফাইল | অবস্থা |
|---|---|---|
| Layer-shell surface (top, exclusive zone = panel height) | `src/main.c` | ✅ আসল code |
| Mux logo (vector-এ আঁকা, logo.svg-র geometry) + click-এ menu | `src/main.c`, `src/draw.c` | ✅ |
| Mux menu: About / Settings / Sleep / Restart / Shut Down / Log Out — command config থেকে | `src/main.c` | ✅ (menu আলাদা overlay layer surface) |
| চলমান app-এর নাম (foreign-toplevel, activated handle) | `src/toplevel.c` | ✅ |
| **Global menu** — focused app-এর File/Edit/View… title panel-এ, click-এ dropdown, item-এ click → app-এ action (নিচে আলাদা অংশ) | `src/appmenu.c` | ✅ D-Bus test পাস; labwc click-test বাকি |
| Clock (strftime format config-এ) | `src/main.c` | ✅ |
| Battery % + glyph (charging-এ mint, <20% coral) — `/sys/class/power_supply` | `src/status.c` | ✅ |
| Network label (Wi-Fi/Ethernet/Offline) — `/sys/class/net` | `src/status.c` | ✅ state মাত্র, signal strength না |
| Config parser + রঙ (অনুমোদিত palette) | `src/config.c` | ✅ unit test পাস |
| প্রোটোকল XML vendor করা (layer-shell, foreign-toplevel) | `protocol/` | ✅ wayland-scanner দিয়ে build-এ generate হয় |

## Build

```bash
meson setup build && ninja -C build     # আসল পথ (Arch-এ)
make                                     # meson না থাকলে fallback
make test                                # config + headless draw test
make test-appmenu                        # global menu end-to-end (session bus লাগে)
```

Config: `config/panel.conf` → বসবে `~/.config/muxlike/panel.conf`।
রঙ, height, font, clock format, কোন status দেখাবে, menu-র প্রতিটা
command — সব ওখানে।

## Global menu (macOS-এর মতো)

Panel session bus-এ `com.canonical.AppMenu.Registrar` নামটা নেয়।
App গুলো (আমাদের নিজের app গুলো `globalmenu.py` দিয়ে) নিজের
menubar model `/com/muxdesk/menu/<short>`-এ আর action group
`/com/muxdesk/actions/<short>`-এ export করে Registrar-এ
RegisterWindow ডাকে। Panel focused window-এর app_id →
`GetNameOwner` → যে registration-এর sender ওই owner, তার
`org.gtk.Menus` subscribe করে title গুলো app-এর নামের পরে আঁকে
(নামটা bold, Mac-এর মতো)। Title-এ click → logo menu-র layer
surface-এই dropdown; item click → `org.gtk.Actions.Activate`।
Disabled item DescribeAll থেকে জেনে ধূসর আঁকে।

Registrar signature তিন রূপেই নেয় — spec-এর (u,s,o,o,o), আমাদের
app দের (u,s,o,o) আর canonical (u,s,o) — কারণ GDBus একটা মাত্র
signature বাঁধতে দেয় বলে registrar টা message filter দিয়ে করা।

Menu-ওয়ালা app না থাকলে / bus না থাকলে / registrar নাম অন্যের
দখলে থাকলে panel আগের মতোই শুধু app-এর নাম দেখায় — crash নেই।

Test: `tests/fake-menu-app.py` contract অনুযায়ী menu export +
register করে (Gtk window mode-ও আছে live test-এর জন্য),
`tests/test_appmenu.c` পুরো পথটা যাচাই করে — title/item/enabled
parse, GetMenuForWindow, activation app পর্যন্ত পৌঁছানো, focus
বদল ও app বন্ধ হলে menu সরে যাওয়া। সব পাস (ASan-এও শুধু exit-time
leak, কোনো invalid access না)।

## সৎভাবে যা বাকি / সীমা

- ❌ **Runtime test হয়নি** — এই environment-এ কোনো Wayland compositor
  নেই। Compile + unit test + "display নেই" error path যাচাই হয়েছে;
  labwc-র উপর চালিয়ে দেখা integration chat-এর কাজ।
- Volume icon নেই — PipeWire/WirePlumber ছাড়া সৎভাবে করা যায় না,
  fake icon বসাইনি। পরে pactl/WirePlumber ধরে যোগ হবে।
- Wi-Fi signal strength নেই (nl80211 লাগে) — শুধু connected state।
- Global menu-র dropdown-এ submenu (▸-ওয়ালা item) এখন শুধু দেখায়,
  খোলে না — আমাদের app গুলোর menu সমতল বলে আপাতত লাগে না।
  Keyboard navigation-ও নেই দুই menu-তেই।
- Menu-তে keyboard navigation নেই (keyboard_interactivity=none) —
  click/tap-ই পথ, ফোনের touch-এর জন্য যথেষ্ট; পরে চাইলে যোগ হবে।
- `mux-settings` / `mux-logout` command দুটো Settings app / session
  অংশের binary — ওগুলো না থাকলে menu item চাপলে terminal-এ error
  message আসবে, panel crash করবে না।
- Battery glyph আঁকা (cairo vector) — আসল icon artwork (MuxLike theme)
  এখানে ব্যবহার হয়নি; status আইকন সেট তৈরি হলে বদলানো যাবে।
