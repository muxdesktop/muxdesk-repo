# mux-launcher — MuxLike DE: Launcher ও Search

## সহজ কথায় আগে

আমাদের **নিজের DE** (base: labwc/wlroots, shell নিজের)-এর app খোলার
জায়গা। একটাই program, দুটো মুখ:

- **`mux-launcher --grid`** — Launchpad-ধাঁচের পুরো-পর্দার app grid:
  সব app icon+নামসহ page-এ ভাগ করা, উপরে খোঁজার বাক্স, swipe/dot/
  arrow দিয়ে page বদল।
- **`mux-launcher --search`** — Spotlight-ধাঁচের search: উপরে মাঝখানে
  ভাসমান কার্ড, লিখলেই app মেলে, ফাইলও খোঁজে (hook), Enter-এ খোলে।

একই command আবার চালালে বন্ধ হয় (toggle) — labwc keybind থেকে
এভাবেই ডাকা হয়: **Super+Space = search, Super+A / F4 = grid**
(`config/labwc-snippet.md`)। Panel/Dock-ও একই command ডাকবে।

## বানানো ও চালানো

```bash
# নির্ভরতা (Arch): gtk4, gtk-layer-shell (gtk4), meson, ninja, glib2
meson setup build
meson compile -C build
meson test -C build          # GTK-মুক্ত অংশের পরীক্ষা (compositor লাগে না)
./build/mux-launcher --grid  # labwc session-এর ভিতরে চালাতে হবে
```

Config: `~/.config/muxdesk/mux-launcher.ini` — উদাহরণ
`config/mux-launcher.ini` (icon theme, grid-এর মাপ, ফাইল খোঁজা
চালু/বন্ধ, animation-এর সময়)। ফাইল না থাকলে default চলে।

নকশা ও সিদ্ধান্ত: `DESIGN.md`। Master spec: `../../DESIGN.md` §3ক
(icon/palette FINAL — violet `#7c5cff`, mint `#00c2a8`, coral `#ff6f61`;
icon theme-এর নাম `MuxLike`)।

## তৈরি / বাকি — সৎ তালিকা

### ✅ তৈরি (code লেখা শেষ)

- [x] `.desktop` পড়া — `src/app-model.c`: GDesktopAppInfo দিয়ে সব
      standard জায়গা, NoDisplay/Hidden বাদ, নাম ধরে সাজানো,
      applications folder-এ নজরদারি (নতুন app এলে নিজে refresh)
- [x] Search-এর মাথা — `src/search-engine.c`: score-ভিত্তিক মিল
      (হুবহু > শুরু > শব্দের শুরু > ভিতরে > keyword > subsequence),
      Unicode casefold — **বাংলা নামেও চলে**; ফাইল খোঁজার async hook
      ($HOME, লুকানো folder বাদ, depth/সংখ্যা সীমা, cancellable)
- [x] Config — `src/mux-config.c` (INI, নিরাপদ default)
- [x] Grid window — `src/launcher-window.c`: layer-shell fullscreen
      OVERLAY, page ভাগ (default 7×4), page dot (সক্রিয়টা mint),
      swipe + ←/→ + dot click, উপরে search box (লিখলে filter),
      click-এ launch, Esc-এ বন্ধ
- [x] Search window — `src/search-window.c`: layer-shell panel,
      120ms debounce, app ফলাফল সঙ্গে সঙ্গে + ফাইল ফলাফল async-এ
      যোগ, ↑/↓ বাছাই, Enter-এ launch/খোলা, Esc-এ বন্ধ
- [x] Animation-এর বাস্তব অংশ — `src/animation.c`: খুললে fade-in,
      search কার্ড slide-down (layer margin), ease-out cubic
- [x] Style — `src/style.css`: অনুমোদিত palette, গোল কার্ড, pill search
- [x] Build — `meson.build`, নিজের `data/mux-launcher.desktop`,
      labwc keybind নির্দেশ (`config/labwc-snippet.md`)

### ✅ যাচাই হয়েছে (এই environment-এ)

- [x] **GTK-মুক্ত core compile + test PASS** — `tests/test-core.c`
      8/8: search score (বাংলা prefix-সহ), config default, app model,
      আসল ফাইল বানিয়ে ফাইল-খোঁজা
- [x] **নাম-সাজানো যাচাই** — `tests/test-sort.c`: জানা দুটো .desktop
      (Alpha/Zebra) দিয়ে সাজানো ঠিক কিনা (একটা comparator ভুল
      এই পরীক্ষাতেই ধরা পড়ে ঠিক করা হয়েছে)

### ❌ বাকি / যাচাই হয়নি

- [ ] **GTK অংশের compile ও runtime পরীক্ষা হয়নি** — এই
      environment-এ gtk4/gtk4-layer-shell নেই, labwc-ও নেই।
      Integration chat আসল session-এ test করবে। (Code GTK 4.10+
      API ধরে লেখা; Arch-এর সর্বশেষ gtk4-এ মিলবে।)
- [ ] Grid-এ **folder বানানো** (টেনে app জোড়া দেওয়া) — নেই, পরের ধাপ
- [ ] খোলার **scale/zoom animation** ও বন্ধের **fade-out** — পরিকল্পনা
      `DESIGN.md`-তে; এখন fade-in + slide-down-ই বাস্তব
- [ ] পেছনের **আসল blur** — compositor-এর কাজ; এখন আধা-স্বচ্ছ গাঢ় রঙ
- [ ] ফাইল খোঁজায় **index** — এখন সরাসরি হাঁটে (depth সীমিত বলে চলে);
      hook-এর পেছনে পরে index বসবে, UI বদলাবে না
- [ ] **MuxLike icon artwork** — master spec অনুযায়ী আঁকা বাকি;
      ততদিন inherit theme থেকে fallback icon আসে
- [ ] MuxDesktop app-এর নিজের Wayland server-এ চলা — ওই server-এ
      `zwlr_layer_shell_v1` থাকা চাই (integration-এর বিষয়,
      `config/labwc-snippet.md`-তে নোট আছে)
