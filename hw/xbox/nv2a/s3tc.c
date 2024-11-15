/*
 * QEMU texture decompression routines
 *
 * Copyright (c) 2020 Wilhelm Kovatch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "s3tc.h"

static inline void decode_bc1_colors(uint16_t c0,
                                     uint16_t c1,
                                     uint8_t r[4],
                                     uint8_t g[4],
                                     uint8_t b[4],
                                     uint8_t a[16],
                                     bool transparent)
{
    r[0] = ((c0 & 0xF800) >> 8) * 0xFF / 0xF8,
    g[0] = ((c0 & 0x07E0) >> 3) * 0xFF / 0xFC,
    b[0] = ((c0 & 0x001F) << 3) * 0xFF / 0xF8,
    a[0] = 255;

    r[1] = ((c1 & 0xF800) >> 8) * 0xFF / 0xF8,
    g[1] = ((c1 & 0x07E0) >> 3) * 0xFF / 0xFC,
    b[1] = ((c1 & 0x001F) << 3) * 0xFF / 0xF8,
    a[1] = 255;

    if (transparent) {
        r[2] = (r[0]+r[1])/2;
        g[2] = (g[0]+g[1])/2;
        b[2] = (b[0]+b[1])/2;
        a[2] = 255;

        r[3] = 0;
        g[3] = 0;
        b[3] = 0;
        a[3] = 0;
    } else {
        r[2] = (2*r[0]+r[1])/3;
        g[2] = (2*g[0]+g[1])/3,
        b[2] = (2*b[0]+b[1])/3;
        a[2] = 255;

        r[3] = (r[0]+2*r[1])/3;
        g[3] = (g[0]+2*g[1])/3;
        b[3] = (b[0]+2*b[1])/3;
        a[3] = 255;
    }
}

static inline void write_block_to_texture(uint8_t *converted_data,
                                          uint32_t indices,
                                          int i, int j, int width,
                                          int z_pos_factor,
                                          uint8_t r[4],
                                          uint8_t g[4],
                                          uint8_t b[4],
                                          uint8_t a[16],
                                          bool separate_alpha)
{
    int x0 = i * 4,
        y0 = j * 4;

    int x1 = x0 + 4,
        y1 = y0 + 4;

    for (int y = y0; y < y1; y++) {
        int y_index = 4 * (y - y0);
        int z_plus_y_pos_factor = z_pos_factor + y * width;
        for (int x = x0; x < x1; x++) {
            int xy_index = y_index + x - x0;
            uint8_t index = (indices >> 2 * xy_index) & 0x03;
            uint8_t alpha_index = separate_alpha ? xy_index : index;
            uint32_t color = (r[index] << 24) | (g[index] << 16) | (b[index] << 8) | a[alpha_index];
            *(uint32_t*)(converted_data + (z_plus_y_pos_factor + x) * 4) = color;
        }
    }
}

static inline void decompress_dxt1_block(const uint8_t block_data[8],
                                         uint8_t *converted_data,
                                         int i, int j, int width,
                                         int z_pos_factor)
{
    uint16_t c0 = ((uint16_t*)block_data)[0],
             c1 = ((uint16_t*)block_data)[1];
    uint8_t r[4], g[4], b[4], a[16];
    decode_bc1_colors(c0, c1, r, g, b, a, c0 <= c1);

    uint32_t indices = ((uint32_t*)block_data)[1];
    write_block_to_texture(converted_data, indices,
                           i, j, width, z_pos_factor,
                           r, g, b, a, false);
}

static inline void decompress_dxt3_block(const uint8_t block_data[16],
                                         uint8_t *converted_data,
                                         int i, int j, int width,
                                         int z_pos_factor)
{
    uint16_t c0 = ((uint16_t*)block_data)[4],
             c1 = ((uint16_t*)block_data)[5];
    uint8_t r[4], g[4], b[4], a[16];
    decode_bc1_colors(c0, c1, r, g, b, a, false);

    uint64_t alpha = ((uint64_t*)block_data)[0];
    for (int a_i=0; a_i < 16; a_i++) {
        a[a_i] = (((alpha >> 4*a_i) & 0x0F) << 4) * 0xFF / 0xF0;
    }

    uint32_t indices = ((uint32_t*)block_data)[3];
    write_block_to_texture(converted_data, indices,
                           i, j, width, z_pos_factor,
                           r, g, b, a, true);
}

static inline void decompress_dxt5_block(const uint8_t block_data[16],
                                         uint8_t *converted_data,
                                         int i, int j, int width,
                                         int z_pos_factor)
{
    uint16_t c0 = ((uint16_t*)block_data)[4],
             c1 = ((uint16_t*)block_data)[5];
    uint8_t r[4], g[4], b[4], a[16];
    decode_bc1_colors(c0, c1, r, g, b, a, false);

    uint64_t alpha = ((uint64_t*)block_data)[0];
    uint8_t a0 = block_data[0];
    uint8_t a1 = block_data[1];
    uint8_t a_palette[8];
    a_palette[0] = a0;
    a_palette[1] = a1;
    if (a0 > a1) {
        a_palette[2] = (6*a0+1*a1)/7;
        a_palette[3] = (5*a0+2*a1)/7;
        a_palette[4] = (4*a0+3*a1)/7;
        a_palette[5] = (3*a0+4*a1)/7;
        a_palette[6] = (2*a0+5*a1)/7;
        a_palette[7] = (1*a0+6*a1)/7;
    } else {
        a_palette[2] = (4*a0+1*a1)/5;
        a_palette[3] = (3*a0+2*a1)/5;
        a_palette[4] = (2*a0+3*a1)/5;
        a_palette[5] = (1*a0+4*a1)/5;
        a_palette[6] = 0;
        a_palette[7] = 255;
    }
    for (int a_i = 0; a_i < 16; a_i++) {
        a[a_i] = a_palette[(alpha >> (16+3*a_i)) & 0x07];
    }

    uint32_t indices = ((uint32_t*)block_data)[3];
    write_block_to_texture(converted_data, indices,
                           i, j, width, z_pos_factor,
                           r, g, b, a, true);
}

uint8_t *decompress_3d_texture_data(GLint color_format,
                                    const uint8_t *data,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth)
{
    assert((width > 0) && (width % 4 == 0));
    assert((height > 0) && (height % 4 == 0));
    assert((depth > 0) && (depth < 4 || depth % 4 == 0));
    int block_depth = MIN(depth, 4);
    int num_blocks_x = width/4,
        num_blocks_y = height/4,
        num_blocks_z = depth/block_depth;
    uint8_t *converted_data = (uint8_t*)g_malloc(width * height * depth * 4);
    for (int k = 0; k < num_blocks_z; k++) {
        for (int j = 0; j < num_blocks_y; j++) {
            for (int i = 0; i < num_blocks_x; i++) {
                for (int slice = 0; slice < block_depth; slice++) {

                    int block_index = k * num_blocks_y * num_blocks_x + j * num_blocks_x + i;
                    int sub_block_index = block_index * block_depth + slice;
                    int z_pos_factor = (k * block_depth + slice) * width * height;

                    if (color_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
                        decompress_dxt1_block(data + 8 * sub_block_index, converted_data,
                                              i, j, width, z_pos_factor);
                    } else if (color_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT) {
                        decompress_dxt3_block(data + 16 * sub_block_index, converted_data,
                                              i, j, width, z_pos_factor);
                    } else if (color_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
                        decompress_dxt5_block(data + 16 * sub_block_index, converted_data,
                                              i, j, width, z_pos_factor);
                    } else {
                        assert(false);
                    }

                }
            }
        }
    }
    return converted_data;
}

uint8_t *decompress_2d_texture_data(GLint color_format, const uint8_t *data,
                                    unsigned int width, unsigned int height)
{
    assert((width > 0) && (width % 4 == 0));
    assert((height > 0) && (height % 4 == 0));
    int num_blocks_x = width / 4, num_blocks_y = height / 4;
    uint8_t *converted_data = (uint8_t *)g_malloc(width * height * 4);
    for (int j = 0; j < num_blocks_y; j++) {
        for (int i = 0; i < num_blocks_x; i++) {
            int block_index = j * num_blocks_x + i;
            if (color_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
                decompress_dxt1_block(data + 8 * block_index,
                                      converted_data, i, j, width, 0);
            } else if (color_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT) {
                decompress_dxt3_block(data + 16 * block_index,
                                      converted_data, i, j, width, 0);
            } else if (color_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
                decompress_dxt5_block(data + 16 * block_index,
                                      converted_data, i, j, width, 0);
            } else {
                assert(false);
            }
        }
    }
    return converted_data;
}
