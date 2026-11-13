project('a52dec', 'c',
        version: '0.8.0',
        license: 'GPL-2.0-or-later',
        default_options: ['warning_level=1', 'c_std=gnu99'])

# liba52 only: the ATSC A/52 (AC-3) decoder, without the a52dec player,
# libao or the test programs. The configuration mirrors what upstream's
# configure derives on a hosted toolchain.

cc = meson.get_compiler('c')
libm = cc.find_library('m', required: false)

conf = configuration_data()
conf.set('PACKAGE', '"a52dec"')
conf.set('VERSION', '"' + meson.project_version() + '"')
if host_machine.endian() == 'big'
  conf.set('WORDS_BIGENDIAN', 1)
endif
if cc.has_header('malloc.h') and cc.has_function('memalign', prefix: '#include <malloc.h>')
  conf.set('HAVE_MEMALIGN', 1)
endif
if cc.compiles('int main(void) { return __builtin_expect(1, 1); }',
               name: '__builtin_expect')
  conf.set('HAVE_BUILTIN_EXPECT', 1)
endif
if cc.compiles('static char x[16] __attribute__((aligned(64))); int main(void) { return x[0]; }',
               name: '__attribute__((aligned))')
  conf.set('ATTRIBUTE_ALIGNED_MAX', 64)
endif
configure_file(output: 'config.h', configuration: conf)

a52_inc = include_directories('include')

liba52 = static_library('a52',
  files(
    'liba52/bitstream.c',
    'liba52/imdct.c',
    'liba52/bit_allocate.c',
    'liba52/parse.c',
    'liba52/downmix.c',
  ),
  include_directories: [include_directories('.'), a52_inc],
  c_args: ['-Wno-unused-parameter', '-Wno-error'],
  dependencies: libm,
  install: false)

a52_dep = declare_dependency(link_with: liba52,
                             include_directories: a52_inc,
                             dependencies: libm)

meson.override_dependency('liba52', a52_dep)
