test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

exe = executable('test-xbox-compat',
                 sources: files('test-compat.c'),
                 dependencies: [qemuutil, glib])

test('xbox-compat', exe,
     env: test_env,
     args: ['--tap', '-k'],
     protocol: 'tap',
     suite: ['xbox', 'xbox-compat'])
