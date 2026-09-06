test_nv2a_ptimer = executable(
  'test-nv2a-ptimer',
  files(
    'test-nv2a-ptimer.c',
    'mock-nv2a-ptimer.c',
    meson.project_source_root() / 'hw/xbox/nv2a/ptimer.c'
  ),
  c_args: [
    '-DCOMPILING_PER_TARGET',
    '-DCONFIG_TARGET="i386-softmmu-config-target.h"',
    '-DCONFIG_DEVICES="i386-softmmu-config-devices.h"',
  ],
  include_directories: [
    include_directories('../../../target/i386'),
  ],
  dependencies: [qemuutil],
  build_by_default: false,
)

test('test-nv2a-ptimer', test_nv2a_ptimer,
     args: ['--tap', '-k'],
     protocol: 'tap',
     suite: ['xbox'])

