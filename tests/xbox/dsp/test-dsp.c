/*
 * DSP tests.
 *
 * Copyright (c) 2025 Matt Borgerson
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

#include "qemu/osdep.h"
#include "hw/xbox/mcpx/apu/dsp/dsp.h"

static void scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr, size_t len, bool dir)
{
    assert(!"Not implemented");
}

static void fifo_rw(void *opaque, uint8_t *ptr, unsigned int index, size_t len, bool dir)
{
    assert(!"Not implemented");
}

static void load_prog(DSPState *s, const char *path)
{
    FILE *file = fopen(path, "r");
    assert(file && "Error opening file");

    char type, line[100], arg1[20], arg2[20];
    int addr, value;

    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%c %s %s", &type, arg1, arg2) >= 2) {
            switch (type) {
                case 'P':
                case 'X':
                case 'Y': {
                    assert(sscanf(arg1, "%x", &addr) == 1);
                    assert(sscanf(arg2, "%x", &value) == 1);
                    dsp_write_memory(s, type, addr, value);
                    break;
                }
            }
        } else {
            printf("Invalid line: %s\n", line);
            assert(0);
        }
    }

    fclose(file);
}

static void test_dsp_basic(void)
{
    g_autofree gchar *path = g_test_build_filename(G_TEST_DIST, "data", "basic", NULL);

    DSPState *s = dsp_init(NULL, scratch_rw, fifo_rw);

    load_prog(s, path);
    dsp_run(s, 1000);

    uint32_t v = dsp_read_memory(s, 'X', 3);
    g_assert_cmphex(v, ==, 0x123456);

    dsp_destroy(s);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/basic", test_dsp_basic);

    return g_test_run();
}
