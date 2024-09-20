imgui_files = files(
  'imgui/imgui.cpp',
  'imgui/imgui_draw.cpp',
  'imgui/imgui_tables.cpp',
  'imgui/imgui_widgets.cpp',
  'imgui/backends/imgui_impl_sdl2.cpp',
  'imgui/backends/imgui_impl_opengl3.cpp',
  'imgui/misc/cpp/imgui_stdlib.cpp',
  #'imgui/imgui_demo.cpp',
)

imgui_cppargs = ['-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM',
                 '-include', 'imgui_impl_opengl3_loader_override.h']

libimgui = static_library('imgui',
                          sources: imgui_files,
                          cpp_args: imgui_cppargs,
                          include_directories: ['.', 'imgui'],
                          dependencies: [sdl, opengl])
imgui = declare_dependency(link_with: libimgui,
                           include_directories: ['imgui', 'imgui/backends'])

implot_files = files(
  'implot/implot.cpp',
  'implot/implot_items.cpp'
  #'implot/implot_demo.cpp',
)

libimplot = static_library('implot',
                           sources: implot_files,
                           include_directories: 'implot',
                           dependencies: [imgui])
implot = declare_dependency(link_with: libimplot,
                            include_directories: 'implot')

noc_ss = ss.source_set()
noc_ss.add(when: 'CONFIG_LINUX', if_true: [gtk, files('noc_file_dialog/noc_file_dialog_gtk.c')])
noc_ss.add(when: 'CONFIG_WIN32', if_true: files('noc_file_dialog/noc_file_dialog_win32.cc'))
noc_ss.add(when: 'CONFIG_DARWIN', if_true: files('noc_file_dialog/noc_file_dialog_macos.m'))
noc_ss = noc_ss.apply(config_all, strict: false)
noclib = static_library('noc',
                        sources: noc_ss.sources(),
                        dependencies: noc_ss.dependencies(),
                        include_directories: 'noc_file_dialog')
noc = declare_dependency(include_directories: 'noc_file_dialog', link_with: noclib)

libstb_image = static_library('stb_image',
                              sources: 'stb_image/stb_image_impl.c')
stb_image = declare_dependency(include_directories: 'stb_image',
                               link_with: libstb_image)

fa = declare_dependency(include_directories: 'fa')

if cpu == 'x86_64'
  libfpng_cpp_args = ['-DFPNG_NO_SSE=0', '-msse4.1', '-mpclmul']
else
  libfpng_cpp_args = ['-DFPNG_NO_SSE=1']
endif

libfpng = static_library('fpng', sources: 'fpng/fpng.cpp', cpp_args: libfpng_cpp_args)
fpng = declare_dependency(include_directories: 'fpng', link_with: libfpng)

json = declare_dependency(include_directories: 'json')
httplib = declare_dependency(include_directories: 'httplib')

libfatx = static_library('fatx', sources: 'fatx/fatx.c')
fatx = declare_dependency(include_directories: 'fatx', link_with: libfatx)
