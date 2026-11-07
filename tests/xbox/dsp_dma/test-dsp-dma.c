#include "qemu/osdep.h"

#include "dsp_dma.h"
#include "dsp_dma_regs.h"
#include "interp/dsp_cpu_regs.h"

#define TRANSFER_WORDS 4
#define DESCRIPTOR_ADDRESS 0
#define SAMPLE_ADDRESS 0x100

typedef struct DMATestSync {
    GMutex mutex;
    GCond cond;
    unsigned int arrived;
} DMATestSync;

typedef struct DMATestState {
    DSPDMAState dma;
    uint32_t memory[0x1800];
    uint32_t capture[TRANSFER_WORDS];
    DMATestSync *sync;
} DMATestState;

static uint32_t mem_read(void *opaque, int space, uint32_t addr)
{
    DMATestState *s = opaque;

    g_assert_cmpint(space, ==, DSP_SPACE_X);
    g_assert_cmpuint(addr, <, G_N_ELEMENTS(s->memory));
    return s->memory[addr];
}

static void mem_write(void *opaque, int space, uint32_t addr, uint32_t value)
{
    DMATestState *s = opaque;

    g_assert_cmpint(space, ==, DSP_SPACE_X);
    g_assert_cmpuint(addr, <, G_N_ELEMENTS(s->memory));
    s->memory[addr] = value;
}

static void scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr,
                       size_t len, bool direction)
{
    g_assert_not_reached();
}

static void fifo_rw(void *opaque, uint8_t *ptr, unsigned int index,
                    size_t len, bool direction)
{
    DMATestState *s = opaque;

    g_assert_cmpuint(index, ==, 0);
    g_assert_cmpuint(len, ==, sizeof(s->capture));
    g_assert_true(direction);

    if (s->sync) {
        g_mutex_lock(&s->sync->mutex);
        s->sync->arrived++;
        if (s->sync->arrived == 2) {
            g_cond_broadcast(&s->sync->cond);
        }
        while (s->sync->arrived < 2) {
            g_cond_wait(&s->sync->cond, &s->sync->mutex);
        }
        g_mutex_unlock(&s->sync->mutex);
    }

    memcpy(s->capture, ptr, len);
}

static void init_dma(DMATestState *s, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->dma.mem_opaque = s;
    s->dma.mem_read = mem_read;
    s->dma.mem_write = mem_write;
    s->dma.rw_opaque = s;
    s->dma.scratch_rw = scratch_rw;
    s->dma.fifo_rw = fifo_rw;

    s->memory[DESCRIPTOR_ADDRESS] = NODE_POINTER_EOL;
    s->memory[DESCRIPTOR_ADDRESS + 1] =
        NODE_CONTROL_DIRECTION | (2 << 10);
    s->memory[DESCRIPTOR_ADDRESS + 2] = TRANSFER_WORDS;
    s->memory[DESCRIPTOR_ADDRESS + 3] = SAMPLE_ADDRESS;
    for (unsigned int i = 0; i < TRANSFER_WORDS; i++) {
        s->memory[SAMPLE_ADDRESS + i] = seed + i;
    }
}

static void run_dma(DMATestState *s)
{
    dsp_dma_write(&s->dma, DMA_NEXT_BLOCK, DESCRIPTOR_ADDRESS);
    dsp_dma_write(&s->dma, DMA_CONTROL, DMA_CONTROL_ACTION_START);
}

static gpointer run_dma_thread(gpointer opaque)
{
    run_dma(opaque);
    return NULL;
}

static void test_per_instance_scratch_buffer(void)
{
    DMATestState first;
    DMATestState second;
    DMATestSync sync;

    init_dma(&first, 0x00110000);
    init_dma(&second, 0x00220000);

    run_dma(&first);
    g_assert_nonnull(first.dma.scratch_buf);

    g_mutex_init(&sync.mutex);
    g_cond_init(&sync.cond);
    sync.arrived = 0;
    first.sync = &sync;
    second.sync = &sync;

    GThread *first_thread =
        g_thread_new("dsp-dma-first", run_dma_thread, &first);
    GThread *second_thread =
        g_thread_new("dsp-dma-second", run_dma_thread, &second);
    g_thread_join(first_thread);
    g_thread_join(second_thread);

    g_assert_nonnull(second.dma.scratch_buf);
    g_assert_true(first.dma.scratch_buf != second.dma.scratch_buf);
    for (unsigned int i = 0; i < TRANSFER_WORDS; i++) {
        g_assert_cmphex(first.capture[i], ==, 0x00110000 + i);
        g_assert_cmphex(second.capture[i], ==, 0x00220000 + i);
    }

    dsp_dma_cleanup(&first.dma);
    dsp_dma_cleanup(&second.dma);
    g_assert_null(first.dma.scratch_buf);
    g_assert_null(second.dma.scratch_buf);
    g_assert_cmpuint(first.dma.scratch_buf_size, ==, 0);
    g_assert_cmpuint(second.dma.scratch_buf_size, ==, 0);
    g_cond_clear(&sync.cond);
    g_mutex_clear(&sync.mutex);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/xbox/dsp/dma/per-instance-scratch",
                    test_per_instance_scratch_buffer);
    return g_test_run();
}
