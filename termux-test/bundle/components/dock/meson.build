project('mux-dock', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

wayland_client = dependency('wayland-client')
wayland_scanner = find_program('wayland-scanner')
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
m = meson.get_compiler('c').find_library('m')

proto_srcs = []
foreach name : ['wlr-layer-shell-unstable-v1',
                'wlr-foreign-toplevel-management-unstable-v1']
  xml = files('protocol' / (name + '.xml'))
  header = custom_target(name + '-header',
    input: xml, output: name + '-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
  code = custom_target(name + '-code',
    input: xml, output: name + '-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  proto_srcs += [header, code]
endforeach

# Same xdg_popup link note as mux-panel: real xdg-shell when
# wayland-protocols is installed, vendored type stub otherwise.
wp = dependency('wayland-protocols', required: false)
if wp.found()
  xdg_xml = wp.get_variable(pkgconfig: 'pkgdatadir') / 'stable/xdg-shell/xdg-shell.xml'
else
  xdg_xml = files('protocol/xdg-popup-types.xml')
endif
proto_srcs += custom_target('xdg-shell-code',
  input: xdg_xml, output: 'xdg-shell-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])

executable('mux-dock',
  ['src/main.c', 'src/config.c', 'src/draw.c', 'src/apps.c',
   'src/magnify.c', 'src/toplevel.c'] + proto_srcs,
  dependencies: [wayland_client, cairo, pangocairo, glib, gobject, m],
  install: true)

install_data('config/dock.conf',
  install_dir: get_option('datadir') / 'muxlike')

test_magnify = executable('test-magnify',
  ['tests/test_magnify.c', 'src/magnify.c'],
  dependencies: [m])
test('magnify', test_magnify)

test_apps = executable('test-apps',
  ['tests/test_apps.c', 'src/apps.c'],
  dependencies: [glib, gobject])
test('apps', test_apps)

test_config = executable('test-config',
  ['tests/test_config.c', 'src/config.c'],
  dependencies: [glib, gobject])
test('config', test_config)

test_draw = executable('test-draw',
  ['tests/test_draw.c', 'src/draw.c', 'src/config.c'],
  dependencies: [wayland_client, cairo, pangocairo, glib, gobject])
test('draw', test_draw)
