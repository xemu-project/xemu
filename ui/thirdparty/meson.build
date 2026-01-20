noc_ss = ss.source_set()
if host_os == 'linux'
  noc_ss.add([gtk, files('noc_file_dialog/noc_file_dialog_gtk.c')])
elif host_os == 'windows'
  noc_ss.add(files('noc_file_dialog/noc_file_dialog_win32.cc'))
elif host_os == 'darwin'
  noc_ss.add(files('noc_file_dialog/noc_file_dialog_macos.m'))
endif
noc_ss = noc_ss.apply(config_target, strict: false)
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

json = dependency('nlohmann_json', required: true)
libfatx = static_library('fatx', sources: files('fatx/fatx.c') + genh)
fatx = declare_dependency(include_directories: 'fatx', link_with: libfatx)
