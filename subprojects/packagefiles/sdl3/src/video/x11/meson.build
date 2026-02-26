
sources += files(
  'SDL_x11clipboard.c',
  'SDL_x11dyn.c',
  'SDL_x11events.c',
  'SDL_x11framebuffer.c',
  'SDL_x11keyboard.c',
  'SDL_x11messagebox.c',
  'SDL_x11modes.c',
  'SDL_x11mouse.c',
  'SDL_x11opengl.c',
  'SDL_x11opengles.c',
  'SDL_x11pen.c',
  'SDL_x11settings.c',
  'SDL_x11shape.c',
  'SDL_x11toolkit.c',
  'SDL_x11touch.c',
  'SDL_x11video.c',
  'SDL_x11vulkan.c',
  'SDL_x11window.c',
  'SDL_x11xfixes.c',
  'SDL_x11xinput2.c',
  'SDL_x11xsync.c',
  'SDL_x11xtest.c',
  'edid-parse.c',
  'xsettings-client.c',
)


xfixes_dep = dependency(
  'xfixes',
  required: false,
)
xcursor_dep = dependency(
  'xcursor',
  required: false,
)
xrandr_dep = dependency(
  'xrandr',
  required: false,
)

if xfixes_dep.found()
  sdl_deps += xfixes_dep
endif
if xcursor_dep.found()
  cdata.set('SDL_VIDEO_DRIVER_X11_XCURSOR', 1)
  sdl_deps += xcursor_dep
endif
if xrandr_dep.found()
  cdata.set('SDL_VIDEO_DRIVER_X11_XRANDR', 1)
  sdl_deps += xrandr_dep
endif


have_XSYNC_header = cc.has_header('X11/extensions/sync.h')
have_XSS_header = cc.has_header('X11/extensions/scrnsaver.h')
have_XSHAPE_header = cc.has_header('X11/extensions/shape.h')
have_XTEST_header = cc.has_header('X11/extensions/XTest.h')
have_XDBE_header = cc.has_header('X11/extensions/Xdbe.h')
have_XEXT_header = cc.has_header('X11/extensions/Xext.h')

have_shmat_in_libc_code = '''
  #include <sys/shm.h>
  int main(int argc, char **argv) { shmat;}
'''
have_shmat_in_libc = cc.compiles(
  have_shmat_in_libc_code,
  name: 'have shmat in libc',
)

if not have_shmat_in_libc
  cdata.set('NO_SHARED_MEMORY', 1)
endif

have_X11_genericevent_code = '''
  #include <X11/Xlib.h>
  int main(int argc, char **argv) {
  Display *display;
  XEvent event;
  XGenericEventCookie *cookie = &event.xcookie;
  XNextEvent(display, &event);
  XGetEventData(display, cookie);
  XFreeEventData(display, cookie);
  return 0; }
'''
have_X11_genericevent = cc.compiles(
  have_X11_genericevent_code,
  name: 'have OpenGL genericevent support',
)

if have_X11_genericevent
  cdata.set('SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS', 1)
endif


# X11/XKBlib is not used within CMake script


# X11 scroll info
have_XInput2_ScrollInfo_code = '''
  #include <X11/Xlib.h>
  #include <X11/Xproto.h>
  #include <X11/extensions/XInput2.h>
  XIScrollClassInfo *s;
  int main(int argc, char **argv) {}
'''
have_XInput2_ScrollInfo = cc.compiles(
  have_XInput2_ScrollInfo_code,
  name: 'have XInput2 scrollinfo',
)

if have_XInput2_ScrollInfo
  cdata.set('SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO', 1)
endif


# X11 multitouch

have_XInput2_multitouch_code = '''
  #include <X11/Xlib.h>
  #include <X11/Xproto.h>
  #include <X11/extensions/XInput2.h>
  int event_type = XI_TouchBegin;
  XITouchClassInfo *t;
  Status XIAllowTouchEvents(Display *a,int b,unsigned int c,Window d,int f) {
    return (Status)0;
  }
  int main(int argc, char **argv) { return 0; }
'''
have_XInput2_multitouch = cc.compiles(
  have_XInput2_multitouch_code,
  name: 'have XInput2 multi-touch',
)

if have_XInput2_multitouch
  cdata.set('SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH', 1)
endif


# X11 touch gestures

have_XInput2_gesture_code = '''
  #include <X11/Xlib.h>
  #include <X11/Xproto.h>
  #include <X11/extensions/XInput2.h>
  int event_type = XI_GesturePinchBegin;
  XITouchClassInfo *t;
  Status XIAllowTouchEvents(Display *a,int b,unsigned int c,Window d,int f) {
    return (Status)0;
  }
  int main(int argc, char **argv) { return 0; }
'''
have_XInput2_gesture = cc.compiles(
  have_XInput2_gesture_code,
  name: 'have XInput2 touch gesture',
)

if have_XInput2_gesture
  cdata.set('SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_GESTURE', 1)
endif

