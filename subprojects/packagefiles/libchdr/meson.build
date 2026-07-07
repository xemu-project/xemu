project('libchdr', 'c',
        version: '0.3.0',
        default_options: ['default_library=static'])

libchdr_inc = include_directories(
  'include',
  'src',
  'deps/lzma-25.01/include',
  'deps/miniz-3.1.1',
  'deps/zstd-1.5.7',
)

libchdr_sources = files(
  'deps/lzma-25.01/src/LzmaDec.c',
  'deps/miniz-3.1.1/miniz.c',
  'deps/zstd-1.5.7/zstddeclib.c',
  'src/libchdr_bitstream.c',
  'src/libchdr_cdrom.c',
  'src/libchdr_chd.c',
  'src/libchdr_codec_cdfl.c',
  'src/libchdr_codec_cdlz.c',
  'src/libchdr_codec_cdzl.c',
  'src/libchdr_codec_cdzs.c',
  'src/libchdr_codec_flac.c',
  'src/libchdr_codec_huff.c',
  'src/libchdr_codec_lzma.c',
  'src/libchdr_codec_zlib.c',
  'src/libchdr_codec_zstd.c',
  'src/libchdr_flac.c',
  'src/libchdr_huffman.c',
)

libchdr_args = [
  '-DMINIZ_NO_ARCHIVE_APIS',
  '-DMINIZ_NO_DEFLATE_APIS',
  '-DMINIZ_NO_STDIO',
  '-DMINIZ_NO_TIME',
  '-DVERIFY_BLOCK_CRC=1',
  '-DWANT_RAW_DATA_SECTOR=1',
  '-DWANT_SUBCODE=1',
]

libchdr_lib = static_library(
  'chdr',
  libchdr_sources,
  include_directories: libchdr_inc,
  c_args: libchdr_args,
  override_options: ['warning_level=0', 'werror=false'],
)

libchdr_dep = declare_dependency(
  include_directories: libchdr_inc,
  link_with: libchdr_lib,
)

meson.override_dependency('libchdr', libchdr_dep)
