# mux-launcher — MuxLike DE: Launcher (Launchpad grid) + Search (Spotlight)
# বানানো:  meson setup build && meson compile -C build
# চালানো পরীক্ষা (GTK-মুক্ত অংশ):  meson test -C build
project('mux-launcher', 'c',
  version: '0.1.0',
  default_options: ['c_std=gnu11', 'warning_level=2'])

glib   = dependency('glib-2.0', version: '>=2.72')
gio    = dependency('gio-2.0')
giounix = dependency('gio-unix-2.0')
gobj   = dependency('gobject-2.0')
gtk    = dependency('gtk4', version: '>=4.10')
layershell = dependency('gtk4-layer-shell-0', required: false)
if not layershell.found()
  # Arch-এ package নাম gtk-layer-shell, pkg-config নাম gtk-layer-shell-0 (gtk4)
  layershell = dependency('gtk-layer-shell-0')
endif
m_dep = meson.get_compiler('c').find_library('m', required: false)

datadir = get_option('datadir')

core_sources = files(
  'src/app-model.c',
  'src/search-engine.c',
  'src/mux-config.c',
)

executable('mux-launcher',
  core_sources + files(
    'src/main.c',
    'src/launcher-window.c',
    'src/search-window.c',
    'src/layer-util.c',
    'src/animation.c',
  ),
  dependencies: [glib, gio, giounix, gobj, gtk, layershell, m_dep],
  c_args: ['-DMUX_DATADIR="@0@"'.format(
             get_option('prefix') / datadir)],
  install: true)

install_data('src/style.css',
  install_dir: datadir / 'muxdesk-launcher')
install_data('data/mux-launcher.desktop',
  install_dir: datadir / 'applications')
install_data('config/mux-launcher.ini',
  install_dir: get_option('sysconfdir') / 'muxdesk',
  rename: 'mux-launcher.ini.example')

# --- পরীক্ষা: GTK ছাড়াই চলে (এই environment-এও যাচাই করা গেছে) ---
test_core = executable('test-core',
  core_sources + files('tests/test-core.c'),
  dependencies: [glib, gio, giounix, gobj],
  include_directories: include_directories('src'))
test('core', test_core)

test_sort = executable('test-sort',
  core_sources + files('tests/test-sort.c'),
  dependencies: [glib, gio, giounix, gobj],
  include_directories: include_directories('src'))
test('sort', test_sort,
  env: {'XDG_DATA_DIRS': meson.current_source_dir() / 'tests' / 'fakeapps' + ':/usr/share:/usr/local/share'})
