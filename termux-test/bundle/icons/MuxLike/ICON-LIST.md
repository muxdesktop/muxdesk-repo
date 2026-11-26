# MuxLike icon theme — কোন icon লাগবে (FINAL)

## নিয়ম — FINAL ✅ (চূড়ান্ত অনুমোদন 2026-11-25, v3 sample অনুযায়ী)

> User-এর চূড়ান্ত সিদ্ধান্ত: **Mac-like আকার + আলাদা রঙের palette
> — violet / mint / coral। এটাই final।**
> পার্থক্যের মূল উপায় **রঙ** — ভারী M-ছাপ প্রতিটা icon-এ বাধ্যতামূলক
> **না**। M-motif থাকবে শুধু logo-সংশ্লিষ্ট জায়গায়: **top bar-এর
> MuxDesk logo আর Store icon**। বাকি icon-এ M/Rim বসানো হবে না।

- **আকার/ধাঁচ: Mac-like** — squircle (corner ~22–24%), 512×512 master
  SVG, বড় পরিষ্কার glyph, উজ্জ্বল ভরাট রঙ, নরম ছায়া। Glyph-এর ভঙ্গি
  Mac-ধাঁচেরই (folder, compass-বলয়, gear, খাম, নোট, পাপড়ি)।
- **Palette (final):**

  | নাম | Hex |
  |---|---|
  | Violet | `#7c5cff` |
  | গভীর violet | `#5a3fd4` (gradient-এর গাঢ় প্রান্ত / ছায়া) |
  | Mint | `#00c2a8` |
  | Coral | `#ff6f61` |
  | গাঢ় ভিত্তি | `#12141f` (glyph-এর গাঢ় অংশে সহায়ক) |
  | হালকা | `#e8eaf6` (পাতা/সাদা অংশে সহায়ক) |

- Apple-এর **logo/trademark** icon-এ থাকবে না — এটা কঠোরই থাকল।
- ফাইল: `scalable/apps/mux-<name>.svg` (master) + একই নামে PNG
  (256/128/64/48/32)।
- অবস্থা: **সব ১৮টা app icon + folder/user-home artwork আঁকা হয়ে গেছে**
  (2026-11-25, MuxLike DE Window/Theme/Icon chat): `scalable/apps/mux-*.svg`
  ১০টা + `scalable/places/folder.svg`, `user-home.svg` — সব 512×512 SVG
  master, XML-valid, final palette যাচাই করা। PNG render (256/128/64/48/32)
  এখনো বাকি — SVG থেকেই icon theme চলবে, PNG পরে বানালেই হবে।
  বাকি ৬টা (maps/clock/calculator/weather/text/camera) + video/pdf যোগ হয়ে
  এখন মোট **১৮টা app icon** আঁকা শেষ (2026-11-25 সন্ধ্যা)। শুধু
  `mux-launchpad` icon টা এখনো বাকি; ততদিন inherit (hicolor) fallback চলবে।

## App icon তালিকা — final রঙের মিল (v3 sample)

★ = sample-এ সরাসরি ঠিক করা মিল — বদলানো যাবে না।
বাকিগুলো একই তিন-রঙের পরিবার থেকে নেওয়া, sample-এর ধাঁচে।

| Icon নাম | Launcher / আসল app | আকার (Mac-ধাঁচের) | Final রঙ |
|---|---|---|---|
| `mux-files` | Mux Files / Nautilus | খোলা ফোল্ডার | ★ **mint** — mint ফোল্ডার, হালকা mint ট্যাব |
| `mux-settings` | Mux Settings / GNOME Settings | গিয়ার | ★ **violet** — violet গিয়ার/গা, হালকা মাঝখান |
| `mux-browser` | Mux Browser / Firefox | কম্পাস-বলয় | ★ **violet** — violet বলয়, সাদা/coral ছোট সূঁচ-ছোঁয়া |
| `mux-music` | Mux Music / Lollypop | সুরের নোট | ★ **violet** — violet গা, সাদা নোট |
| `mux-store` | Mux Store / GNOME Software | কেনাকাটার ব্যাগ | ★ **coral** — coral ব্যাগ, গায়ে সাদা **M** (logo-সংশ্লিষ্ট — এখানেই M-motif থাকবে) |
| `mux-calendar` | Mux Calendar / GNOME Calendar | তারিখের পাতা | ★ **coral** — সাদা পাতা, coral ওপরের ফিতে, গাঢ় সংখ্যা |
| `mux-photos` | Mux Photos / Loupe | রঙিন পাপড়ির চাকা | ★ **multicolor** — পাপড়ি **violet + coral + mint** ঘুরিয়ে |
| `mux-notes` | Mux Notes / GNOME Notes | খাতার পাতা | ★ **mint** — mint ওপরের অংশ, সাদা পাতা |
| `mux-terminal` | Mux Terminal / Console | squircle-এ `>_` | ★ **violet** — violet গা (গাঢ় violet ছায়া), সাদা/mint `>_` |
| `mux-mail` | Mux Mail / Geary | খাম | ★ **mint** — mint খাম, সাদা ঢাকনা |
| `mux-maps` | Mux Maps / GNOME Maps | ভাঁজ-করা মানচিত্র + pin | mint/violet মানচিত্রের খণ্ড, coral pin |
| `mux-clock` | Mux Clock / GNOME Clocks | ঘড়ির মুখ | violet গা, সাদা মুখ, গাঢ় কাঁটা |
| `mux-calculator` | Mux Calculator / GNOME Calculator | বোতামের সারি | গাঢ় `#12141f` গা, `=` বোতাম mint, বাকি ধূসর |
| `mux-weather` | Mux Weather / GNOME Weather | সূর্য + মেঘ | violet আকাশ, coral সূর্য, সাদা মেঘ |
| `mux-text` | Mux Text / Text Editor | পাতা + পেন্সিল | সাদা পাতা, violet দাগ ও পেন্সিল |
| `mux-camera` | Mux Camera / Snapshot | ক্যামেরা + লেন্স | mint গা, গাঢ় লেন্স-বলয়, coral বিন্দু |
| `mux-video` | Video Player / Totem-ধাঁচের | সাদা বলয়ে play ত্রিভুজ | গাঢ় `#12141f` গা, mint play, সাদা বলয় |
| `mux-pdf` | PDF Viewer / Evince-ধাঁচের | ভাঁজ-কোণওয়ালা পাতা + PDF লেখা | coral গা, সাদা পাতা, coral লেখা |
| `muxdesk` | top bar / About | মূল project-এর আসল logo হুবহু | violet → mint (আসল logo — logo-সংশ্লিষ্ট, M এখানেই) |
| `mux-launchpad` | App Grid | ৩×৩ ছোট squircle সারি | violet/mint/coral ঘুরিয়ে নয়টা |

## System icon তালিকা (পরে — একই palette)

- **Places:** `folder`, `folder-documents`, `folder-download`, `folder-music`,
  `folder-pictures`, `folder-videos`, `user-home`, `user-trash`,
  `folder-remote` — `mux-files`-এর **mint ফোল্ডার**-এর সাথে মিলিয়ে।
- **Status:** `battery-*`, `network-wireless-*`, `audio-volume-*`,
  `bluetooth-*` — প্রথমে hicolor থেকে inherit, নিজের সেট পরে।
- **MimeTypes:** `text-x-generic`, `image-x-generic`, `video-x-generic`,
  `audio-x-generic`, `application-pdf` — সাধারণ আকার, palette-এর রঙে।
- **Devices:** `drive-harddisk`, `phone`, `computer` — পরে।

Inherits: `hicolor` — নিজের icon না থাকলে ওখান থেকে আসবে,
ভাঙা icon দেখাবে না।
