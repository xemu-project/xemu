test_dsp_thread_safety = executable(
  'test-dsp-thread-safety',
  files(
    'test-dsp-thread-safety.c',
  ),
  c_args: [
    '-DCOMPILING_PER_TARGET',
    '-DCONFIG_TARGET="i386-softmmu-config-target.h"',
    '-DCONFIG_DEVICES="i386-softmmu-config-devices.h"',
  ],
  include_directories: [
    include_directories('../../../target/i386'),
    include_directories('../../../hw/xbox/mcpx/apu/dsp'),
    include_directories('../../../hw/xbox/mcpx/apu/dsp/interp'),
  ],
  dependencies: [qemuutil],
  link_with: libdsp_cpu_interp,
  build_by_default: false,
)

test_env = environment()
if host_os == 'darwin'
  macos_arch = host_machine.cpu() == 'aarch64' ? 'arm64' : host_machine.cpu()
  test_env.prepend('DYLD_FALLBACK_LIBRARY_PATH', meson.project_source_root() / 'macos-libs' / macos_arch / 'opt/local/lib')
  test_env.prepend('DYLD_LIBRARY_PATH', meson.project_source_root() / 'macos-libs' / macos_arch / 'opt/local/lib')
endif

test('test-dsp-thread-safety', test_dsp_thread_safety,
     args: ['--tap', '-k'],
     protocol: 'tap',
     env: test_env,
     suite: ['xbox'])
