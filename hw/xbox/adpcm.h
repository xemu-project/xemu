/*
 * ADPCM decoder
 *
 * Copyright (c) 2017 Jannik Vogel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


// See https://wiki.multimedia.cx/index.php/IMA_ADPCM for more information

#include <stdint.h>
#include <assert.h>

static int8_t ima_index_table[16] = {
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
}; 

static uint16_t ima_step_table[89] = { 
      7,     8,     9,    10,    11,    12,    13,    14,    16,    17, 
     19,    21,    23,    25,    28,    31,    34,    37,    41,    45, 
     50,    55,    60,    66,    73,    80,    88,    97,   107,   118, 
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066, 
   2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
   5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899, 
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 
};

typedef struct {
  int32_t predictor;
  int8_t step_index;
  uint16_t step;
} ADPCMDecoder;

static void adpcm_decoder_initialize(ADPCMDecoder* d, int16_t predictor, int8_t step_index) {
  d->predictor = predictor;
  d->step_index = step_index;
}

// The upper portion of the `nibble` argument is ignored.
static int16_t adpcm_decoder_step(ADPCMDecoder* d, uint8_t nibble) {

  // Get step and prepare index for next sample
  if (d->step_index < 0) {
    d->step_index = 0;
  } else if (d->step_index > 88) {
    d->step_index = 88;
  }
  d->step = ima_step_table[d->step_index];
  d->step_index += ima_index_table[nibble & 0xF];

  // Calculate diff
  int32_t diff = d->step >> 3;
  if (nibble & 1) {
    diff += d->step >> 2;
  }
  if (nibble & 2) {
    diff += d->step >> 1;
  }
  if (nibble & 4) {
    diff += d->step;
  }
  if (nibble & 8) {
    diff = -diff;
  }

  // Update predictor and clamp to signed 16 bit
  d->predictor += diff;
  if (d->predictor < -0x8000) {
    d->predictor = -0x8000;
  } else if (d->predictor > 0x7FFF) {
    d->predictor = 0x7FFF;
  }

  return d->predictor;
}
