project('inih',
    ['c','cpp'],
    meson_version : '>= 0.46.0',
    default_options : ['default_library=both'],
    license : 'BSD-3-Clause',
    version : '48'
)

pkg = import('pkgconfig')

#### inih ####
install_headers('ini.h')

inc_inih = include_directories('.')

lib_inih = library('inih',
    ['ini.c'],
    include_directories : inc_inih,
    install : true,
    version : meson.project_version(),
    soversion : '0'
)

pkg.generate(lib_inih,
    name : 'inih',
    description : 'simple .INI file parser',
    version : meson.project_version()
)

inih_dep = declare_dependency(
    link_with : lib_inih,
    include_directories : inc_inih
)

#### INIReader ####
install_headers('cpp/INIReader.h')

inc_INIReader = include_directories('cpp')

lib_INIReader = library('INIReader',
    ['cpp/INIReader.cpp'],
    include_directories : inc_INIReader,
    dependencies : inih_dep,
    install : true,
    version : meson.project_version(),
    soversion : '0'
)

pkg.generate(lib_INIReader,
    name : 'INIReader',
    description : 'simple .INI file parser for C++',
    version : meson.project_version()
)

INIReader_dep = declare_dependency(
    link_with : lib_inih,
    include_directories : inc_INIReader
)
