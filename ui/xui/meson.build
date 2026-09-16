xemu_ss.add(files(
  'actions.cc',
  'animation.cc',
  'compat.cc',
  'debug.cc',
  'font-manager.cc',
  'gl-helpers.cc',
  'input-manager.cc',
  'main-menu.cc',
  'main.cc',
  'misc.cc',
  'menubar.cc',
  'monitor.cc',
  'notifications.cc',
  'popup-menu.cc',
  'reporting.cc',
  'scene-components.cc',
  'scene-manager.cc',
  'scene.cc',
  'snapshot-manager.cc',
  'viewport-manager.cc',
  'welcome.cc',
  'widgets.cc',
))

if host_os == 'windows'
  xemu_ss.add(files('d3d11-corewindow-present.cc', 'd3d11-present.cc',
                    'update.cc', 'win32-dxgi-present.cc'))
endif

if get_option('xbox_uwp')
  subdir('uwp')
endif
