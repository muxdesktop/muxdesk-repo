# labwc-তে জোড়া দেওয়া — Launcher ও Search

MuxLike DE-র compositor **labwc** (wlroots)। `mux-launcher` একটাই
binary — labwc keybind থেকে ডাকলেই grid/search toggle হয়।

## keybind — `~/.config/labwc/rc.xml`-এর `<keyboard>` অংশে যোগ করো

```xml
<!-- Super+Space → Spotlight-ধাঁচের search -->
<keybind key="W-space">
  <action name="Execute" command="mux-launcher --search" />
</keybind>

<!-- Super+A বা F4 → Launchpad grid -->
<keybind key="W-a">
  <action name="Execute" command="mux-launcher --grid" />
</keybind>
<keybind key="F4">
  <action name="Execute" command="mux-launcher --grid" />
</keybind>
```

> labwc-তে `W` = Super/Logo key। একই keybind আবার চাপলে launcher
> নিজেই বন্ধ হয় (toggle) — আলাদা close keybind লাগে না। Esc-ও বন্ধ করে।

## autostart — `~/.config/labwc/autostart`

Launcher আগে থেকে চালিয়ে রাখার দরকার **নেই** — প্রথম keybind-এই
process উঠে বাসিন্দা হয়ে থাকে, পরের বার সঙ্গে সঙ্গে খোলে।
তবু session শুরুতেই গরম রাখতে চাইলে:

```sh
mux-launcher --grid &   # প্রথম ডাকেই grid খুলে যাবে — সাধারণত চাই না
```

তাই autostart-এ **না রাখাই** সুপারিশ। শুধু panel/dock component
থেকে grid খুলতে চাইলে ওরাও একই command চালাবে
(`mux-launcher --grid` / `--search`)।

## session-এ বসানো (Arch package হিসেবে এলে)

- binary: `/usr/bin/mux-launcher`
- CSS: `/usr/share/muxdesk-launcher/style.css`
- নিজের .desktop: `/usr/share/applications/mux-launcher.desktop`
  (অন্য launcher-এও যাতে দেখা যায়; নিজের grid-এ NoDisplay না —
  নিজেকে দেখানো ঠিক আছে)
- config উদাহরণ: `/etc/muxdesk/mux-launcher.ini.example`

## MuxDesktop app-এর নিজের Wayland server নিয়ে সৎ নোট

এই component **wlr-layer-shell** protocol ধরে নেয় — labwc সেটা দেয়।
MuxDesktop app-এর built-in Wayland server (Display Server component)
যদি layer-shell না দেয়, launcher সেখানে চলবে না — তখন ওই server-এ
`zwlr_layer_shell_v1` যোগ করা integration-এর কাজ, এই component-এর না।
Main/integration chat-এ এই কথাটা test-এর সময় মাথায় রাখতে হবে।
