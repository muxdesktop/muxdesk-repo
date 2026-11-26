# mux-dock — MuxLike DE-র নিচের floating dock

সহজ কথায়: এটা আমাদের **নিজের লেখা** dock — Dash to Dock / plank
কিছুই না। mux-panel-এর মতোই C + `wlr-layer-shell` + cairo (wl_shm),
labwc compositor-এর উপর বসে। নিচে মাঝখানে floating, pointer গেলে
icon বড় হয় (magnification), চলমান app-এর নিচে dot, click-এ
launch/focus।

## কী কী আছে

| অংশ | ফাইল | অবস্থা |
|---|---|---|
| Layer-shell surface (bottom anchor, মাঝখানে floating, TOP layer) | `src/main.c` | ✅ আসল code |
| Magnification — cosine falloff, layout relaxation | `src/magnify.c` | ✅ unit test পাস |
| Pinned app: `.desktop` parse (standard applications dirs), NoDisplay/Hidden বাদ | `src/apps.c` | ✅ unit test পাস |
| Running app merge + dot (active হলে violet) — foreign-toplevel | `src/toplevel.c`, `src/main.c` | ✅ code; runtime বাকি |
| Click: running → activate, না হলে launch (Exec field code ঝাড়া) | `src/main.c`, `src/apps.c` | ✅ |
| প্রথম tile: Mux logo → `mux-launcher` চালায় | `src/main.c` | ✅ |
| Icon tile: অনুমোদিত palette অনুযায়ী রঙ (files mint, store/calendar coral, settings/browser/music/terminal violet, photos gradient) + app-এর আদ্যক্ষর | `src/main.c` | ✅ অস্থায়ী চেহারা (নিচে দেখো) |
| Icon= আসল `.png` path হলে সেই ছবিই আঁকে | `src/main.c` | ✅ |
| Config (আকার, magnification, রঙ, pinned তালিকা) | `src/config.c`, `config/dock.conf` | ✅ unit test পাস |

## Build

```bash
meson setup build && ninja -C build     # আসল পথ (Arch-এ)
make                                     # meson না থাকলে fallback
make test                                # magnify + apps + config + draw test
```

Config: `config/dock.conf` → বসবে `~/.config/muxlike/dock.conf`।
`reserve_space = true` দিলে dock জায়গা রিজার্ভ করবে (exclusive
zone); default floating — window-এর উপর ভাসে, জায়গা নেয় না।

## সৎভাবে যা বাকি / সীমা

- ❌ **Runtime test হয়নি** — এই environment-এ Wayland compositor
  নেই। Compile, ৪টা unit test আর "display নেই" error path যাচাই
  হয়েছে; labwc-তে চালিয়ে দেখা integration chat-এর কাজ।
- **আসল icon artwork এখনো tile দিয়ে চলছে।** MuxLike icon theme-এর
  SVG গুলো আঁকা হয়েছে বলে master spec জানাচ্ছে, কিন্তু dock এখন
  theme থেকে SVG/PNG খুঁজে আঁকে না — Icon= সরাসরি `.png` path হলেই
  ছবি আসে, নাহলে palette tile + আদ্যক্ষর। Icon theme lookup
  (index.theme ধরে খোঁজা + SVG rasterize) পরের ধাপ — librsvg/
  gdk-pixbuf dependency যোগ করে করতে হবে।
- Tooltip নেই (app-এর নাম hover-এ দেখায় না) — পরের ধাপ।
- Launch-এ bounce animation নেই; click-এর feedback শুধু app খোলা।
- একই app-এর একাধিক window থাকলে click-এ প্রথমটাই activate হয় —
  window বাছাইয়ের ছোট তালিকা (macOS-এর মতো) বাকি।
- Drag দিয়ে icon সাজানো / dock থেকে unpin করা যায় না — pinned
  তালিকা config file-এই বদলাতে হয়।
- Position শুধু bottom — config-এ left/right নেই (MacLike নকশায়
  dock নিচেই)।
- Magnification-এর layout দুই pass relaxation — pointer-এর ঠিক
  নিচের icon max-এর *কাছাকাছি* পৌঁছায়, হুবহু max না (test-এ এই
  আচরণটাই ধরা আছে)। চলমান ব্যবহারে pointer নড়তে থাকায় পার্থক্য
  চোখে পড়ে না।
