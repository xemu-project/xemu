sources += files(
  'SDL_waylandclipboard.c',
  'SDL_waylandcolor.c',
  'SDL_waylanddatamanager.c',
  'SDL_waylanddyn.c',
  'SDL_waylandevents.c',
  'SDL_waylandkeyboard.c',
  'SDL_waylandmessagebox.c',
  'SDL_waylandmouse.c',
  'SDL_waylandopengles.c',
  'SDL_waylandshmbuffer.c',
  'SDL_waylandvideo.c',
  'SDL_waylandvulkan.c',
  'SDL_waylandwindow.c',
)

# parse XKB version to avoid issues within SDL3 config
xkdbcommon_version = xkbcommon_dep.version().split('.')

xkdbcommon_major = xkdbcommon_version[0].to_int()
xkdbcommon_minor = xkdbcommon_version[1].to_int()
xkdbcommon_patch = xkdbcommon_version[2].to_int()

cdata.set('SDL_XKBCOMMON_VERSION_MAJOR', xkdbcommon_major)
cdata.set('SDL_XKBCOMMON_VERSION_MINOR', xkdbcommon_minor)
cdata.set('SDL_XKBCOMMON_VERSION_PATCH', xkdbcommon_patch)
