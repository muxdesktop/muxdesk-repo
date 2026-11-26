# Mux Files — নিজের file manager (GTK4)

## সহজ কথায়
আগে Mux Files ছিল Nautilus-এর launcher। এখন এটা **আমাদের নিজের app** —
নিজের code, নিজের window। Nautilus লাগে না।

## কেন Python + PyGObject (GTK4), C নয়
- Arch-এ `python-gobject + gtk4` তৈরি প্যাকেজ; compile ছাড়াই চলে, ফোনে
  build toolchain টানতে হয় না।
- একই code labwc/wlroots-এও, GNOME-এও চলে (খাঁটি Wayland GTK4 app)।
- C-তে একই কাজ ৩–৪ গুণ code, লাভ শুধু startup-এর কয়েক ms — file manager-এ
  সেটা মূল্যবান না। পরে দরকার হলে নির্দিষ্ট অংশ C-তে নেওয়া যাবে।

## চালানো
```sh
# নির্ভরতা (Arch/MuxDesk): sudo pacman -S python python-gobject gtk4
./mux-files            # dev tree থেকে সরাসরি
# বা বসিয়ে: ../install-own-apps.sh && mux-files
```

## তৈরি ✅
- Window + unified headerbar (44px): back/forward যুগ্ম pill, sidebar toggle,
  breadcrumb, search, grid/list segmented toggle, sort menu, নতুন folder বোতাম
- Traffic-light বান্ধব: window বোতাম বামে (`close,minimize,maximize:` decoration layout)
- Sidebar (218px, row 32px): Home, Documents, Downloads, Pictures, Music, Videos, Trash
- Grid view (icon 48px) + List view (নাম/আকার/ধরন/তারিখ column)
- নিচে slim bar (26px ধাঁচ): বামে "N items · X GB free", পাশে **path bar**
  (লিখে Enter = সেখানে যাওয়া, Ctrl+L)
- Search (Ctrl+F): নাম দিয়ে live filter
- Sort: নাম/আকার/তারিখ, folder আগে
- ফাইল অপারেশন: খোলা (default app), নতুন folder, rename (F2), Trash (Delete),
  Properties, ডান-click menu, keyboard: Ctrl+F / Ctrl+L / F9 (sidebar)
- **Global menu (panel):** menubar (File: New Folder/Properties/Close,
  Edit: Rename/Trash, View: Grid/List + Sort ▸ + Search, Help: About)
  DBus-এ export (`/com/muxdesk/menu/files` + `/com/muxdesk/actions/files`),
  AppMenu Registrar-এ RegisterWindow; registrar না থাকলেও app চলে
  (`mux_files/globalmenu.py`, তিন app-এ একই helper)
- CSS: নিজের `mux_files/data/mux-files.css` — অনুমোদিত spacing/radius/palette
  (violet নির্বাচন), frosted আভা alpha background দিয়ে

## বাকি / সৎ সীমা ❌
- Runtime test হয়নি — এই পরিবেশে GTK/gi নেই, Android SDK-ও নেই।
  যাচাই: `python -m py_compile` (সব ফাইল পাস) + pure-helper test।
  আসল Wayland session-এ প্রথম চালানো Integration chat-এর test-এ বাকি।
- বড় folder sync পড়া হয় (enumerate_children) — হাজার-হাজার ফাইলের
  folder-এ প্রথম খোলা ধীর হতে পারে; async loading বাকি।
- Thumbnail নেই — সব ফাইলে theme icon; ছবির preview বাকি।
- Drag-and-drop, tab, দুই-pane, undo — নেই।
- আসল blur (frosted glass) compositor-এর কাজ; app শুধু alpha background
  দেয় — labwc-তে blur না থাকলে হালকা স্বচ্ছই দেখাবে।
- Trash/Recent আসল GNOME-এর মতো পূর্ণ না: Trash খোলে (gvfs থাকলে),
  Recent/Starred sidebar-এ রাখা হয়নি (ভুয়া জায়গা বানানো হয়নি)।
