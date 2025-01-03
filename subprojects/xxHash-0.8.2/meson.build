project(
    'xxHash',
    ['c'],
    # library files are BSD licensed. xxhsum utility is GPL licensed.
    license: 'BSD-2-Clause and GPL-2.0',
    version: '0.8.2',
    meson_version: '>=0.49.0'
)

cc = meson.get_compiler('c')

force_memory_access = {
    'attribute': 1,
    'unaligned-reads': 2,
    'byteshift': 3,
}

vector = {
    'scalar': 'XXH_SCALAR',
    'sse2': 'XXH_SSE2',
    'avx2': 'XXH_AVX2',
    'avx512': 'XXH_AVX512',
    'neon': 'XXH_NEON',
    'vsx': 'XXH_VSX',
}

c_args = []
c_build_args = []
c_use_args = []

if get_option('32-endjmp')
    c_args += '-DXXH32_ENDJMP'
endif

if get_option('inline-all')
    c_args += '-DXXH_INLINE_ALL'
endif

if get_option('namespace') != ''
    c_args += '-DXXH_NAMESPACE=@0@'.format(get_option('namespace'))
endif

if get_option('force-memory-access') != 'default'
    c_args += '-DXXH_FORCE_MEMORY_ACCESS=@0@'.format(force_memory_access[get_option('force-memory-access')])
endif

if not get_option('force-align-check').auto()
    c_args += '-DXXH_FORCE_ALIGN_CHECK=@0@'.format(get_option('force-align-check').enabled() ? 1 : 0)
endif

if get_option('vector') != 'default'
    c_args += '-DXXH_VECTOR=@0@'.format(vector[get_option('vector')])
endif

if not get_option('prefetch')
    c_args += '-DXXH_NO_PREFETCH'
endif

if get_option('prefetch-dist') > 0
    c_args += '-DXXH_PREFETCH_DIST=@0@'.format(get_option('prefetch-dist'))
endif

if cc.get_id() == 'gcc' and cc.version().version_compare('>=12.0.0') and get_option('optimization') == 'g'
    c_args += '-DXXH_NO_INLINE_HINTS=1'
    message('Ignoring value of inline-hints, see https://github.com/Cyan4973/xxHash/pull/720')
elif not get_option('inline-hints').auto()
    c_args += '-DXXH_NO_INLINE_HINTS=@0@'.format(get_option('inline-hints').enabled() ? 0 : 1)
endif

if not get_option('long-long')
    c_args += '-DXXH_NO_LONG_LONG'
endif

if not get_option('cpu-little-endian').auto()
    c_args += '-DXXH_CPU_LITTLE_ENDIAN=@0@'.format(get_option('cpu-little-endian').enabled() ? 1 : 0)
endif

if not get_option('xxh3')
    c_args += '-DXXH_NO_XXH3'
endif

if host_machine.system() == 'windows'
    c_args += '-DWIN32'

    # On Windows/MinGW, we need to define -DXXH_EXPORT and -DXXH_IMPORT
    if get_option('default_library') != 'static'
        c_build_args += '-DXXH_EXPORT'
        c_use_args += '-DXXH_IMPORT'
    endif
endif

pkgconfig_data = configuration_data({
    'PREFIX': get_option('prefix'),
    'EXECPREFIX': get_option('prefix'),
    'INCLUDEDIR': get_option('includedir'),
    'LIBDIR': get_option('libdir'),
    'VERSION': meson.project_version(),
})

pkgconfig = configure_file(
    input: 'libxxhash.pc.in',
    output: 'libxxhash.pc',
    configuration: pkgconfig_data,
)

install_data(
    pkgconfig,
    install_dir: get_option('prefix') / get_option('libdir') / 'pkgconfig'
)

inc = include_directories('.')

xxhash = library(
    'xxhash',
    ['xxhash.c'],
    version: meson.project_version(),
    c_args: c_args + c_build_args,
    include_directories: [
        inc,
    ],
    install: true,
)

# For use is xxhsum only
xxhash_internal_dep = declare_dependency(
    link_with: xxhash,
    compile_args: c_args + c_use_args,
    include_directories: [
        inc,
    ],
)

if get_option('default_library') == 'static'
    c_args += '-DXXH_STATIC_LINKING_ONLY'
endif

xxhash_dep = declare_dependency(
    link_with: xxhash,
    compile_args: c_args,
    include_directories: [
        inc,
    ],
)

if meson.version().version_compare('>=0.54.0')
    meson.override_dependency('libxxhash', xxhash_dep)
endif

if get_option('cli')
    install_man('cli/xxhsum.1')

    cli_c_args = []

    cli_sources = [
        files(
            'cli/xsum_bench.c',
            'cli/xsum_os_specific.c',
            'cli/xsum_output.c',
            'cli/xsum_sanity_check.c',
            'cli/xxhsum.c',
        ),
    ]

    if get_option('dispatch')
        cli_c_args += '-DXXH_DISPATCH'
        cli_sources += files(
            'xxh_x86dispatch.c',
        )
    endif

    xxhsum = executable(
        'xxhsum',
        cli_sources,
        c_args: cli_c_args,
        dependencies: [
            xxhash_internal_dep,
        ],
        install: true,
    )

    meson.override_find_program('xxhsum', xxhsum)
endif
