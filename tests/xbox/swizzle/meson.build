# swizzle-test.c crosschecks and benchmarks a "method A", which is the swizzle
# code in hw/xbox/nv2a/pgraph (the Makefile renames its symbols with objcopy).
test_nv2a_swizzle = executable(
  'test-nv2a-swizzle',
  files(
    'swizzle-test.c',
    meson.project_source_root() / 'hw/xbox/nv2a/pgraph/swizzle.c',
  ),
  c_args: [
    '-Dswizzle_box_A=swizzle_box',
    '-Dunswizzle_box_A=unswizzle_box',
  ],
  build_by_default: false,
)

test('test-nv2a-swizzle', test_nv2a_swizzle,
     suite: ['xbox'])
