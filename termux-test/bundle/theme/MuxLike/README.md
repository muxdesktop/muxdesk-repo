# MuxLike theme — labwc window decoration

- `labwc/themerc` + `labwc/*.xbm` — আসল labwc theme। বোতাম বামে
  (rc.xml-এর titlebar layout), রঙ: close লাল `#ff5f57`,
  minimize হলুদ `#febc2e`, maximize সবুজ `#28c840` — traffic-light।
  এগুলো macOS-এর রঙের কাছাকাছি হলেও **আমাদের আঁকা XBM mask**,
  Apple-এর কোনো asset না।
- `buttons-svg/` — একই বোতামের SVG source (নথি ও ভবিষ্যতের জন্য);
  labwc নিজে XBM-ই পড়ে।
- Titlebar: গাঢ় `#232338`, লেখা সাদা-আভা, font sans Bold 11।

## গোল corner — সৎ সীমা

- labwc 0.8+ হলে rc.xml-এর `<cornerRadius>12</cornerRadius>`-এ
  server-side window-র corner গোল হয়।
- পুরনো labwc-তে ওই key উপেক্ষিত — corner চৌকোই থাকবে, ভাঙবে না।
- যেসব app নিজে decoration আঁকে (CSD, যেমন GTK4 app), তাদের corner
  app-এর নিজের — compositor/theme দিয়ে বদলানো যায় না।
- XBM বোতাম এক-রঙা mask বলে traffic-light সমতল রঙের হবে —
  gradient/ঝলক (macOS-এর মতো glossy) SSD-তে সম্ভব না।
