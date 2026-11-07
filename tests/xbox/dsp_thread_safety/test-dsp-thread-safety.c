#include "qemu/osdep.h"

#include "interp/dsp_cpu.h"

#define CORE_COUNT 8
#define INSTRUCTION_COUNT 1024

typedef struct DSPThreadTest {
    GMutex mutex;
    GCond cond;
    unsigned int ready;
    dsp_core_t cores[CORE_COUNT];
} DSPThreadTest;

typedef struct DSPThread {
    DSPThreadTest *test;
    dsp_core_t *core;
} DSPThread;

static gpointer execute_nops(gpointer opaque)
{
    DSPThread *thread = opaque;

    g_mutex_lock(&thread->test->mutex);
    thread->test->ready++;
    if (thread->test->ready == CORE_COUNT) {
        g_cond_broadcast(&thread->test->cond);
    }
    while (thread->test->ready < CORE_COUNT) {
        g_cond_wait(&thread->test->cond, &thread->test->mutex);
    }
    g_mutex_unlock(&thread->test->mutex);

    dsp56k_reset_cpu(thread->core);
    for (unsigned int i = 0; i < INSTRUCTION_COUNT; i++) {
        dsp56k_execute_instruction(thread->core);
    }
    return NULL;
}

static void test_concurrent_cores(void)
{
    DSPThreadTest *test = g_new0(DSPThreadTest, 1);
    DSPThread threads[CORE_COUNT];
    GThread *workers[CORE_COUNT];

    g_mutex_init(&test->mutex);
    g_cond_init(&test->cond);

    for (unsigned int i = 0; i < CORE_COUNT; i++) {
        threads[i].test = test;
        threads[i].core = &test->cores[i];
        workers[i] = g_thread_new("dsp-core", execute_nops, &threads[i]);
    }
    for (unsigned int i = 0; i < CORE_COUNT; i++) {
        g_thread_join(workers[i]);
        g_assert_cmpuint(test->cores[i].pc, ==, INSTRUCTION_COUNT);
        g_assert_cmpuint(test->cores[i].opcode_cache[0].op, ==, 0);
        g_assert_nonnull(test->cores[i].opcode_cache[0].entry);
    }

    const void *second_entry = test->cores[1].opcode_cache[0].entry;
    test->cores[0].opcode_cache[0].entry = NULL;
    g_assert_true(test->cores[1].opcode_cache[0].entry == second_entry);

    g_cond_clear(&test->cond);
    g_mutex_clear(&test->mutex);
    g_free(test);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/xbox/dsp/cpu/concurrent-cores", test_concurrent_cores);
    return g_test_run();
}
