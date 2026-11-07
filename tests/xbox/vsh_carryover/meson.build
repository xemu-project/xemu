test_nv2a_vsh_carryover = executable(
  'test-nv2a-vsh-carryover',
  files(
    'test-nv2a-vsh-carryover.c',
    'mock-pgraph.c',
    meson.project_source_root() / 'hw/xbox/nv2a/pgraph/vertex.c',
  ),
  c_args: [
    '-DCOMPILING_PER_TARGET',
    '-DCONFIG_TARGET="i386-softmmu-config-target.h"',
    '-DCONFIG_DEVICES="i386-softmmu-config-devices.h"',
  ],
  include_directories: [
    include_directories('../../../target/i386'),
    include_directories('../../../hw/xbox/nv2a/pgraph'),
  ],
  dependencies: [qemuutil, nv2a_vsh_cpu],
  build_by_default: false,
)

test_env = environment()
if host_os == 'darwin'
  macos_arch = host_machine.cpu() == 'aarch64' ? 'arm64' : host_machine.cpu()
  test_env.prepend('DYLD_FALLBACK_LIBRARY_PATH', meson.project_source_root() / 'macos-libs' / macos_arch / 'opt/local/lib')
  test_env.prepend('DYLD_LIBRARY_PATH', meson.project_source_root() / 'macos-libs' / macos_arch / 'opt/local/lib')
endif

test('test-nv2a-vsh-carryover', test_nv2a_vsh_carryover,
     args: ['--tap', '-k'],
     protocol: 'tap',
     env: test_env,
     suite: ['xbox'])
