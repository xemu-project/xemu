test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

exe = executable('test-dsp',
                 sources: files('test-dsp.c'),
                 dependencies: [dsp, glib])

test('dsp', exe,
     env: test_env,
     args: ['--tap', '-k'],
     protocol: 'tap',
     suite: ['dsp'])
