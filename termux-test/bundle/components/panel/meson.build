project('mux-panel', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

wayland_client = dependency('wayland-client')
wayland_scanner = find_program('wayland-scanner')
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
gobject = dependency('gobject-2.0')

# Protocols are vendored (protocol/*.xml) so the build does not depend
# on where the host keeps wayland-protocols / wlroots XML files.
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

# layer-shell references the xdg_popup type; link the real xdg-shell
# protocol code when wayland-protocols is installed (normal on Arch),
# otherwise the vendored type stub (see protocol/xdg-popup-types.xml).
wp = dependency('wayland-protocols', required: false)
if wp.found()
  xdg_xml = wp.get_variable(pkgconfig: 'pkgdatadir') / 'stable/xdg-shell/xdg-shell.xml'
else
  xdg_xml = files('protocol/xdg-popup-types.xml')
endif
proto_srcs += custom_target('xdg-shell-code',
  input: xdg_xml, output: 'xdg-shell-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])

executable('mux-panel',
  ['src/main.c', 'src/config.c', 'src/draw.c', 'src/status.c',
   'src/toplevel.c', 'src/appmenu.c'] + proto_srcs,
  dependencies: [wayland_client, cairo, pangocairo, glib, gobject, gio],
  install: true)

install_data('config/panel.conf',
  install_dir: get_option('datadir') / 'muxlike')

# Unit tests for the display-independent logic.
test_config = executable('test-config',
  ['tests/test_config.c', 'src/config.c'],
  dependencies: [glib, gobject])
test('config', test_config)

test_draw = executable('test-draw',
  ['tests/test_draw.c', 'src/draw.c', 'src/config.c'],
  dependencies: [wayland_client, cairo, pangocairo, glib, gobject])
test('draw', test_draw)

test_appmenu = executable('test-appmenu',
  ['tests/test_appmenu.c', 'src/appmenu.c'],
  dependencies: [gio, glib, gobject])
# Needs a session bus + the python fake app; run via
# tests/run-appmenu-test.sh rather than plain `meson test`.
