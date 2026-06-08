test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

exe = executable('test-xbox-mcpx-dsp',
                 sources: files('test-dsp.c'),
                 dependencies: [qemuutil, dsp, glib])

test('xbox-mcpx-dsp', exe,
     env: test_env,
     args: ['--tap', '-k'],
     protocol: 'tap',
     suite: ['xbox', 'xbox-mcpx', 'xbox-mcpx-dsp'])

alias_target('test-xbox', exe)
