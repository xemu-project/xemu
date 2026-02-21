files = [
  'alpha-modifier-v1.xml',
  'color-management-v1.xml',
  'cursor-shape-v1.xml',
  'fractional-scale-v1.xml',
  'frog-color-management-v1.xml',
  'idle-inhibit-unstable-v1.xml',
  'input-timestamps-unstable-v1.xml',
  'keyboard-shortcuts-inhibit-unstable-v1.xml',
  'pointer-constraints-unstable-v1.xml',
  'pointer-gestures-unstable-v1.xml',
  'pointer-warp-v1.xml',
  'primary-selection-unstable-v1.xml',
  'relative-pointer-unstable-v1.xml',
  'tablet-v2.xml',
  'text-input-unstable-v3.xml',
  'viewporter.xml',
  'wayland.xml',
  'xdg-activation-v1.xml',
  'xdg-decoration-unstable-v1.xml',
  'xdg-dialog-v1.xml',
  'xdg-foreign-unstable-v2.xml',
  'xdg-output-unstable-v1.xml',
  'xdg-shell.xml',
  'xdg-toplevel-icon-v1.xml',
]

wlheadergen = generator(
  wayland_scanner,
  arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
  output: '@BASENAME@-client-protocol.h',
)
wlcodegen = generator(
  wayland_scanner,
  arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
  output: '@BASENAME@-client-protocol.c',
)

wlheaders = wlheadergen.process(files)
wlsources = wlcodegen.process(files)
sources += wlheaders
sources += wlsources
