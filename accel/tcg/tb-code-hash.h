#ifndef EXEC_TB_CODE_HASH_H
#define EXEC_TB_CODE_HASH_H

#include "exec/cpu-common.h"
#include "accel/tcg/cpu-ldst-common.h"
#include "accel/tcg/cpu-mmu-index.h"
#include "qemu/fast-hash.h"

static inline uint32_t cpu_ldub_code(CPUArchState *env, vaddr addr)
{
    CPUState *cs = env_cpu(env);
    MemOpIdx oi = make_memop_idx(MO_UB, cpu_mmu_index(cs, true));
    return cpu_ldb_code_mmu(env, addr, oi, 0);
}

static inline void cpu_ld_code(CPUArchState *env, vaddr addr, size_t len, uint8_t *out)
{
    for (size_t i = 0; i < len; i++) {
        out[i] = cpu_ldub_code(env, addr+i);
    }
}

static inline uint64_t tb_code_hash_func(CPUArchState *env, vaddr pc, size_t size)
{
    assert(size < 4096);
    uint8_t code[size];
    cpu_ld_code(env, pc, size, code); /* Speed, error handling */
    return fast_hash(code, size);
}

#endif
