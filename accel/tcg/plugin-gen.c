/*
 * plugin-gen.c - TCG-related bits of plugin infrastructure
 *
 * Copyright (C) 2018, Emilio G. Cota <cota@braap.org>
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 *
 * We support instrumentation at an instruction granularity. That is,
 * if a plugin wants to instrument the memory accesses performed by a
 * particular instruction, it can just do that instead of instrumenting
 * all memory accesses. Thus, in order to do this we first have to
 * translate a TB, so that plugins can decide what/where to instrument.
 *
 * Injecting the desired instrumentation could be done with a second
 * translation pass that combined the instrumentation requests, but that
 * would be ugly and inefficient since we would decode the guest code twice.
 * Instead, during TB translation we add "empty" instrumentation calls for all
 * possible instrumentation events, and then once we collect the instrumentation
 * requests from plugins, we either "fill in" those empty events or remove them
 * if they have no requests.
 *
 * When "filling in" an event we first copy the empty callback's TCG ops. This
 * might seem unnecessary, but it is done to support an arbitrary number
 * of callbacks per event. Take for example a regular instruction callback.
 * We first generate a callback to an empty helper function. Then, if two
 * plugins register one callback each for this instruction, we make two copies
 * of the TCG ops generated for the empty callback, substituting the function
 * pointer that points to the empty helper function with the plugins' desired
 * callback functions. After that we remove the empty callback's ops.
 *
 * Note that the location in TCGOp.args[] of the pointer to a helper function
 * varies across different guest and host architectures. Instead of duplicating
 * the logic that figures this out, we rely on the fact that the empty
 * callbacks point to empty functions that are unique pointers in the program.
 * Thus, to find the right location we just have to look for a match in
 * TCGOp.args[]. This is the main reason why we first copy an empty callback's
 * TCG ops and then fill them in; regardless of whether we have one or many
 * callbacks for that event, the logic to add all of them is the same.
 *
 * When generating more than one callback per event, we make a small
 * optimization to avoid generating redundant operations. For instance, for the
 * second and all subsequent callbacks of an event, we do not need to reload the
 * CPU's index into a TCG temp, since the first callback did it already.
 */
#include "qemu/osdep.h"
#include "tcg/tcg.h"
#include "tcg/tcg-op.h"
#include "exec/exec-all.h"
#include "exec/plugin-gen.h"
#include "exec/translator.h"

#ifdef CONFIG_SOFTMMU
# define CONFIG_SOFTMMU_GATE 1
#else
# define CONFIG_SOFTMMU_GATE 0
#endif

/*
 * plugin_cb_start TCG op args[]:
 * 0: enum plugin_gen_from
 * 1: enum plugin_gen_cb
 * 2: set to 1 for mem callback that is a write, 0 otherwise.
 */

enum plugin_gen_from {
    PLUGIN_GEN_FROM_TB,
    PLUGIN_GEN_FROM_INSN,
    PLUGIN_GEN_FROM_MEM,
    PLUGIN_GEN_AFTER_INSN,
    PLUGIN_GEN_N_FROMS,
};

enum plugin_gen_cb {
    PLUGIN_GEN_CB_UDATA,
    PLUGIN_GEN_CB_INLINE,
    PLUGIN_GEN_CB_MEM,
    PLUGIN_GEN_ENABLE_MEM_HELPER,
    PLUGIN_GEN_DISABLE_MEM_HELPER,
    PLUGIN_GEN_N_CBS,
};

/*
 * These helpers are stubs that get dynamically switched out for calls
 * direct to the plugin if they are subscribed to.
 */
void HELPER(plugin_vcpu_udata_cb)(uint32_t cpu_index, void *udata)
{ }

void HELPER(plugin_vcpu_mem_cb)(unsigned int vcpu_index,
                                qemu_plugin_meminfo_t info, uint64_t vaddr,
                                void *userdata)
{ }

static void do_gen_mem_cb(TCGv vaddr, uint32_t info)
{
    TCGv_i32 cpu_index = tcg_temp_new_i32();
    TCGv_i32 meminfo = tcg_const_i32(info);
    TCGv_i64 vaddr64 = tcg_temp_new_i64();
    TCGv_ptr udata = tcg_const_ptr(NULL);

    tcg_gen_ld_i32(cpu_index, cpu_env,
                   -offsetof(ArchCPU, env) + offsetof(CPUState, cpu_index));
    tcg_gen_extu_tl_i64(vaddr64, vaddr);

    gen_helper_plugin_vcpu_mem_cb(cpu_index, meminfo, vaddr64, udata);

    tcg_temp_free_ptr(udata);
    tcg_temp_free_i64(vaddr64);
    tcg_temp_free_i32(meminfo);
    tcg_temp_free_i32(cpu_index);
}

static void gen_empty_udata_cb(void)
{
    TCGv_i32 cpu_index = tcg_temp_new_i32();
    TCGv_ptr udata = tcg_const_ptr(NULL); /* will be overwritten later */

    tcg_gen_ld_i32(cpu_index, cpu_env,
                   -offsetof(ArchCPU, env) + offsetof(CPUState, cpu_index));
    gen_helper_plugin_vcpu_udata_cb(cpu_index, udata);

    tcg_temp_free_ptr(udata);
    tcg_temp_free_i32(cpu_index);
}

/*
 * For now we only support addi_i64.
 * When we support more ops, we can generate one empty inline cb for each.
 */
static void gen_empty_inline_cb(void)
{
    TCGv_i64 val = tcg_temp_new_i64();
    TCGv_ptr ptr = tcg_const_ptr(NULL); /* overwritten later */

    tcg_gen_ld_i64(val, ptr, 0);
    /* pass an immediate != 0 so that it doesn't get optimized away */
    tcg_gen_addi_i64(val, val, 0xdeadface);
    tcg_gen_st_i64(val, ptr, 0);
    tcg_temp_free_ptr(ptr);
    tcg_temp_free_i64(val);
}

static void gen_empty_mem_cb(TCGv addr, uint32_t info)
{
    do_gen_mem_cb(addr, info);
}

/*
 * Share the same function for enable/disable. When enabling, the NULL
 * pointer will be overwritten later.
 */
static void gen_empty_mem_helper(void)
{
    TCGv_ptr ptr;

    ptr = tcg_const_ptr(NULL);
    tcg_gen_st_ptr(ptr, cpu_env, offsetof(CPUState, plugin_mem_cbs) -
                                 offsetof(ArchCPU, env));
    tcg_temp_free_ptr(ptr);
}

static void gen_plugin_cb_start(enum plugin_gen_from from,
                                enum plugin_gen_cb type, unsigned wr)
{
    tcg_gen_plugin_cb_start(from, type, wr);
}

static void gen_wrapped(enum plugin_gen_from from,
                        enum plugin_gen_cb type, void (*func)(void))
{
    gen_plugin_cb_start(from, type, 0);
    func();
    tcg_gen_plugin_cb_end();
}

static void plugin_gen_empty_callback(enum plugin_gen_from from)
{
    switch (from) {
    case PLUGIN_GEN_AFTER_INSN:
        gen_wrapped(from, PLUGIN_GEN_DISABLE_MEM_HELPER,
                    gen_empty_mem_helper);
        break;
    case PLUGIN_GEN_FROM_INSN:
        /*
         * Note: plugin_gen_inject() relies on ENABLE_MEM_HELPER being
         * the first callback of an instruction
         */
        gen_wrapped(from, PLUGIN_GEN_ENABLE_MEM_HELPER,
                    gen_empty_mem_helper);
        /* fall through */
    case PLUGIN_GEN_FROM_TB:
        gen_wrapped(from, PLUGIN_GEN_CB_UDATA, gen_empty_udata_cb);
        gen_wrapped(from, PLUGIN_GEN_CB_INLINE, gen_empty_inline_cb);
        break;
    default:
        g_assert_not_reached();
    }
}

union mem_gen_fn {
    void (*mem_fn)(TCGv, uint32_t);
    void (*inline_fn)(void);
};

static void gen_mem_wrapped(enum plugin_gen_cb type,
                            const union mem_gen_fn *f, TCGv addr,
                            uint32_t info, bool is_mem)
{
    enum qemu_plugin_mem_rw rw = get_plugin_meminfo_rw(info);

    gen_plugin_cb_start(PLUGIN_GEN_FROM_MEM, type, rw);
    if (is_mem) {
        f->mem_fn(addr, info);
    } else {
        f->inline_fn();
    }
    tcg_gen_plugin_cb_end();
}

void plugin_gen_empty_mem_callback(TCGv addr, uint32_t info)
{
    union mem_gen_fn fn;

    fn.mem_fn = gen_empty_mem_cb;
    gen_mem_wrapped(PLUGIN_GEN_CB_MEM, &fn, addr, info, true);

    fn.inline_fn = gen_empty_inline_cb;
    gen_mem_wrapped(PLUGIN_GEN_CB_INLINE, &fn, 0, info, false);
}

static TCGOp *find_op(TCGOp *op, TCGOpcode opc)
{
    while (op) {
        if (op->opc == opc) {
            return op;
        }
        op = QTAILQ_NEXT(op, link);
    }
    return NULL;
}

static TCGOp *rm_ops_range(TCGOp *begin, TCGOp *end)
{
    TCGOp *ret = QTAILQ_NEXT(end, link);

    QTAILQ_REMOVE_SEVERAL(&tcg_ctx->ops, begin, end, link);
    return ret;
}

/* remove all ops until (and including) plugin_cb_end */
static TCGOp *rm_ops(TCGOp *op)
{
    TCGOp *end_op = find_op(op, INDEX_op_plugin_cb_end);

    tcg_debug_assert(end_op);
    return rm_ops_range(op, end_op);
}

static TCGOp *copy_op_nocheck(TCGOp **begin_op, TCGOp *op)
{
    *begin_op = QTAILQ_NEXT(*begin_op, link);
    tcg_debug_assert(*begin_op);
    op = tcg_op_insert_after(tcg_ctx, op, (*begin_op)->opc);
    memcpy(op->args, (*begin_op)->args, sizeof(op->args));
    return op;
}

static TCGOp *copy_op(TCGOp **begin_op, TCGOp *op, TCGOpcode opc)
{
    op = copy_op_nocheck(begin_op, op);
    tcg_debug_assert((*begin_op)->opc == opc);
    return op;
}

static TCGOp *copy_extu_i32_i64(TCGOp **begin_op, TCGOp *op)
{
    if (TCG_TARGET_REG_BITS == 32) {
        /* mov_i32 */
        op = copy_op(begin_op, op, INDEX_op_mov_i32);
        /* mov_i32 w/ $0 */
        op = copy_op(begin_op, op, INDEX_op_mov_i32);
    } else {
        /* extu_i32_i64 */
        op = copy_op(begin_op, op, INDEX_op_extu_i32_i64);
    }
    return op;
}

static TCGOp *copy_mov_i64(TCGOp **begin_op, TCGOp *op)
{
    if (TCG_TARGET_REG_BITS == 32) {
        /* 2x mov_i32 */
        op = copy_op(begin_op, op, INDEX_op_mov_i32);
        op = copy_op(begin_op, op, INDEX_op_mov_i32);
    } else {
        /* mov_i64 */
        op = copy_op(begin_op, op, INDEX_op_mov_i64);
    }
    return op;
}

static TCGOp *copy_const_ptr(TCGOp **begin_op, TCGOp *op, void *ptr)
{
    if (UINTPTR_MAX == UINT32_MAX) {
        /* mov_i32 */
        op = copy_op(begin_op, op, INDEX_op_mov_i32);
        op->args[1] = tcgv_i32_arg(tcg_constant_i32((uintptr_t)ptr));
    } else {
        /* mov_i64 */
        op = copy_op(begin_op, op, INDEX_op_mov_i64);
        op->args[1] = tcgv_i64_arg(tcg_constant_i64((uintptr_t)ptr));
    }
    return op;
}

static TCGOp *copy_extu_tl_i64(TCGOp **begin_op, TCGOp *op)
{
    if (TARGET_LONG_BITS == 32) {
        /* extu_i32_i64 */
        op = copy_extu_i32_i64(begin_op, op);
    } else {
        /* mov_i64 */
        op = copy_mov_i64(begin_op, op);
    }
    return op;
}

static TCGOp *copy_ld_i64(TCGOp **begin_op, TCGOp *op)
{
    if (TCG_TARGET_REG_BITS == 32) {
        /* 2x ld_i32 */
        op = copy_op(begin_op, op, INDEX_op_ld_i32);
        op = copy_op(begin_op, op, INDEX_op_ld_i32);
    } else {
        /* ld_i64 */
        op = copy_op(begin_op, op, INDEX_op_ld_i64);
    }
    return op;
}

static TCGOp *copy_st_i64(TCGOp **begin_op, TCGOp *op)
{
    if (TCG_TARGET_REG_BITS == 32) {
        /* 2x st_i32 */
        op = copy_op(begin_op, op, INDEX_op_st_i32);
        op = copy_op(begin_op, op, INDEX_op_st_i32);
    } else {
        /* st_i64 */
        op = copy_op(begin_op, op, INDEX_op_st_i64);
    }
    return op;
}

static TCGOp *copy_add_i64(TCGOp **begin_op, TCGOp *op, uint64_t v)
{
    if (TCG_TARGET_REG_BITS == 32) {
        /* all 32-bit backends must implement add2_i32 */
        g_assert(TCG_TARGET_HAS_add2_i32);
        op = copy_op(begin_op, op, INDEX_op_add2_i32);
        op->args[4] = tcgv_i32_arg(tcg_constant_i32(v));
        op->args[5] = tcgv_i32_arg(tcg_constant_i32(v >> 32));
    } else {
        op = copy_op(begin_op, op, INDEX_op_add_i64);
        op->args[2] = tcgv_i64_arg(tcg_constant_i64(v));
    }
    return op;
}

static TCGOp *copy_st_ptr(TCGOp **begin_op, TCGOp *op)
{
    if (UINTPTR_MAX == UINT32_MAX) {
        /* st_i32 */
        op = copy_op(begin_op, op, INDEX_op_st_i32);
    } else {
        /* st_i64 */
        op = copy_st_i64(begin_op, op);
    }
    return op;
}

static TCGOp *copy_call(TCGOp **begin_op, TCGOp *op, void *empty_func,
                        void *func, int *cb_idx)
{
    /* copy all ops until the call */
    do {
        op = copy_op_nocheck(begin_op, op);
    } while (op->opc != INDEX_op_call);

    /* fill in the op call */
    op->param1 = (*begin_op)->param1;
    op->param2 = (*begin_op)->param2;
    tcg_debug_assert(op->life == 0);
    if (*cb_idx == -1) {
        int i;

        /*
         * Instead of working out the position of the callback in args[], just
         * look for @empty_func, since it should be a unique pointer.
         */
        for (i = 0; i < MAX_OPC_PARAM_ARGS; i++) {
            if ((uintptr_t)(*begin_op)->args[i] == (uintptr_t)empty_func) {
                *cb_idx = i;
                break;
            }
        }
        tcg_debug_assert(i < MAX_OPC_PARAM_ARGS);
    }
    op->args[*cb_idx] = (uintptr_t)func;
    op->args[*cb_idx + 1] = (*begin_op)->args[*cb_idx + 1];

    return op;
}

/*
 * When we append/replace ops here we are sensitive to changing patterns of
 * TCGOps generated by the tcg_gen_FOO calls when we generated the
 * empty callbacks. This will assert very quickly in a debug build as
 * we assert the ops we are replacing are the correct ones.
 */
static TCGOp *append_udata_cb(const struct qemu_plugin_dyn_cb *cb,
                              TCGOp *begin_op, TCGOp *op, int *cb_idx)
{
    /* const_ptr */
    op = copy_const_ptr(&begin_op, op, cb->userp);

    /* copy the ld_i32, but note that we only have to copy it once */
    begin_op = QTAILQ_NEXT(begin_op, link);
    tcg_debug_assert(begin_op && begin_op->opc == INDEX_op_ld_i32);
    if (*cb_idx == -1) {
        op = tcg_op_insert_after(tcg_ctx, op, INDEX_op_ld_i32);
        memcpy(op->args, begin_op->args, sizeof(op->args));
    }

    /* call */
    op = copy_call(&begin_op, op, HELPER(plugin_vcpu_udata_cb),
                   cb->f.vcpu_udata, cb_idx);

    return op;
}

static TCGOp *append_inline_cb(const struct qemu_plugin_dyn_cb *cb,
                               TCGOp *begin_op, TCGOp *op,
                               int *unused)
{
    /* const_ptr */
    op = copy_const_ptr(&begin_op, op, cb->userp);

    /* ld_i64 */
    op = copy_ld_i64(&begin_op, op);

    /* add_i64 */
    op = copy_add_i64(&begin_op, op, cb->inline_insn.imm);

    /* st_i64 */
    op = copy_st_i64(&begin_op, op);

    return op;
}

static TCGOp *append_mem_cb(const struct qemu_plugin_dyn_cb *cb,
                            TCGOp *begin_op, TCGOp *op, int *cb_idx)
{
    enum plugin_gen_cb type = begin_op->args[1];

    tcg_debug_assert(type == PLUGIN_GEN_CB_MEM);

    /* const_i32 == mov_i32 ("info", so it remains as is) */
    op = copy_op(&begin_op, op, INDEX_op_mov_i32);

    /* const_ptr */
    op = copy_const_ptr(&begin_op, op, cb->userp);

    /* copy the ld_i32, but note that we only have to copy it once */
    begin_op = QTAILQ_NEXT(begin_op, link);
    tcg_debug_assert(begin_op && begin_op->opc == INDEX_op_ld_i32);
    if (*cb_idx == -1) {
        op = tcg_op_insert_after(tcg_ctx, op, INDEX_op_ld_i32);
        memcpy(op->args, begin_op->args, sizeof(op->args));
    }

    /* extu_tl_i64 */
    op = copy_extu_tl_i64(&begin_op, op);

    if (type == PLUGIN_GEN_CB_MEM) {
        /* call */
        op = copy_call(&begin_op, op, HELPER(plugin_vcpu_mem_cb),
                       cb->f.vcpu_udata, cb_idx);
    }

    return op;
}

typedef TCGOp *(*inject_fn)(const struct qemu_plugin_dyn_cb *cb,
                            TCGOp *begin_op, TCGOp *op, int *intp);
typedef bool (*op_ok_fn)(const TCGOp *op, const struct qemu_plugin_dyn_cb *cb);

static bool op_ok(const TCGOp *op, const struct qemu_plugin_dyn_cb *cb)
{
    return true;
}

static bool op_rw(const TCGOp *op, const struct qemu_plugin_dyn_cb *cb)
{
    int w;

    w = op->args[2];
    return !!(cb->rw & (w + 1));
}

static void inject_cb_type(const GArray *cbs, TCGOp *begin_op,
                           inject_fn inject, op_ok_fn ok)
{
    TCGOp *end_op;
    TCGOp *op;
    int cb_idx = -1;
    int i;

    if (!cbs || cbs->len == 0) {
        rm_ops(begin_op);
        return;
    }

    end_op = find_op(begin_op, INDEX_op_plugin_cb_end);
    tcg_debug_assert(end_op);

    op = end_op;
    for (i = 0; i < cbs->len; i++) {
        struct qemu_plugin_dyn_cb *cb =
            &g_array_index(cbs, struct qemu_plugin_dyn_cb, i);

        if (!ok(begin_op, cb)) {
            continue;
        }
        op = inject(cb, begin_op, op, &cb_idx);
    }
    rm_ops_range(begin_op, end_op);
}

static void
inject_udata_cb(const GArray *cbs, TCGOp *begin_op)
{
    inject_cb_type(cbs, begin_op, append_udata_cb, op_ok);
}

static void
inject_inline_cb(const GArray *cbs, TCGOp *begin_op, op_ok_fn ok)
{
    inject_cb_type(cbs, begin_op, append_inline_cb, ok);
}

static void
inject_mem_cb(const GArray *cbs, TCGOp *begin_op)
{
    inject_cb_type(cbs, begin_op, append_mem_cb, op_rw);
}

/* we could change the ops in place, but we can reuse more code by copying */
static void inject_mem_helper(TCGOp *begin_op, GArray *arr)
{
    TCGOp *orig_op = begin_op;
    TCGOp *end_op;
    TCGOp *op;

    end_op = find_op(begin_op, INDEX_op_plugin_cb_end);
    tcg_debug_assert(end_op);

    /* const ptr */
    op = copy_const_ptr(&begin_op, end_op, arr);

    /* st_ptr */
    op = copy_st_ptr(&begin_op, op);

    rm_ops_range(orig_op, end_op);
}

/*
 * Tracking memory accesses performed from helpers requires extra work.
 * If an instruction is emulated with helpers, we do two things:
 * (1) copy the CB descriptors, and keep track of it so that they can be
 * freed later on, and (2) point CPUState.plugin_mem_cbs to the descriptors, so
 * that we can read them at run-time (i.e. when the helper executes).
 * This run-time access is performed from qemu_plugin_vcpu_mem_cb.
 *
 * Note that plugin_gen_disable_mem_helpers undoes (2). Since it
 * is possible that the code we generate after the instruction is
 * dead, we also add checks before generating tb_exit etc.
 */
static void inject_mem_enable_helper(struct qemu_plugin_insn *plugin_insn,
                                     TCGOp *begin_op)
{
    GArray *cbs[2];
    GArray *arr;
    size_t n_cbs, i;

    cbs[0] = plugin_insn->cbs[PLUGIN_CB_MEM][PLUGIN_CB_REGULAR];
    cbs[1] = plugin_insn->cbs[PLUGIN_CB_MEM][PLUGIN_CB_INLINE];

    n_cbs = 0;
    for (i = 0; i < ARRAY_SIZE(cbs); i++) {
        n_cbs += cbs[i]->len;
    }

    plugin_insn->mem_helper = plugin_insn->calls_helpers && n_cbs;
    if (likely(!plugin_insn->mem_helper)) {
        rm_ops(begin_op);
        return;
    }

    arr = g_array_sized_new(false, false,
                            sizeof(struct qemu_plugin_dyn_cb), n_cbs);

    for (i = 0; i < ARRAY_SIZE(cbs); i++) {
        g_array_append_vals(arr, cbs[i]->data, cbs[i]->len);
    }

    qemu_plugin_add_dyn_cb_arr(arr);
    inject_mem_helper(begin_op, arr);
}

static void inject_mem_disable_helper(struct qemu_plugin_insn *plugin_insn,
                                      TCGOp *begin_op)
{
    if (likely(!plugin_insn->mem_helper)) {
        rm_ops(begin_op);
        return;
    }
    inject_mem_helper(begin_op, NULL);
}

/* called before finishing a TB with exit_tb, goto_tb or goto_ptr */
void plugin_gen_disable_mem_helpers(void)
{
    TCGv_ptr ptr;

    if (likely(tcg_ctx->plugin_insn == NULL ||
               !tcg_ctx->plugin_insn->mem_helper)) {
        return;
    }
    ptr = tcg_const_ptr(NULL);
    tcg_gen_st_ptr(ptr, cpu_env, offsetof(CPUState, plugin_mem_cbs) -
                                 offsetof(ArchCPU, env));
    tcg_temp_free_ptr(ptr);
    tcg_ctx->plugin_insn->mem_helper = false;
}

static void plugin_gen_tb_udata(const struct qemu_plugin_tb *ptb,
                                TCGOp *begin_op)
{
    inject_udata_cb(ptb->cbs[PLUGIN_CB_REGULAR], begin_op);
}

static void plugin_gen_tb_inline(const struct qemu_plugin_tb *ptb,
                                 TCGOp *begin_op)
{
    inject_inline_cb(ptb->cbs[PLUGIN_CB_INLINE], begin_op, op_ok);
}

static void plugin_gen_insn_udata(const struct qemu_plugin_tb *ptb,
                                  TCGOp *begin_op, int insn_idx)
{
    struct qemu_plugin_insn *insn = g_ptr_array_index(ptb->insns, insn_idx);

    inject_udata_cb(insn->cbs[PLUGIN_CB_INSN][PLUGIN_CB_REGULAR], begin_op);
}

static void plugin_gen_insn_inline(const struct qemu_plugin_tb *ptb,
                                   TCGOp *begin_op, int insn_idx)
{
    struct qemu_plugin_insn *insn = g_ptr_array_index(ptb->insns, insn_idx);
    inject_inline_cb(insn->cbs[PLUGIN_CB_INSN][PLUGIN_CB_INLINE],
                     begin_op, op_ok);
}

static void plugin_gen_mem_regular(const struct qemu_plugin_tb *ptb,
                                   TCGOp *begin_op, int insn_idx)
{
    struct qemu_plugin_insn *insn = g_ptr_array_index(ptb->insns, insn_idx);
    inject_mem_cb(insn->cbs[PLUGIN_CB_MEM][PLUGIN_CB_REGULAR], begin_op);
}

static void plugin_gen_mem_inline(const struct qemu_plugin_tb *ptb,
                                  TCGOp *begin_op, int insn_idx)
{
    const GArray *cbs;
    struct qemu_plugin_insn *insn = g_ptr_array_index(ptb->insns, insn_idx);

    cbs = insn->cbs[PLUGIN_CB_MEM][PLUGIN_CB_INLINE];
    inject_inline_cb(cbs, begin_op, op_rw);
}

static void plugin_gen_enable_mem_helper(const struct qemu_plugin_tb *ptb,
                                         TCGOp *begin_op, int insn_idx)
{
    struct qemu_plugin_insn *insn = g_ptr_array_index(ptb->insns, insn_idx);
    inject_mem_enable_helper(insn, begin_op);
}

static void plugin_gen_disable_mem_helper(const struct qemu_plugin_tb *ptb,
                                          TCGOp *begin_op, int insn_idx)
{
    struct qemu_plugin_insn *insn = g_ptr_array_index(ptb->insns, insn_idx);
    inject_mem_disable_helper(insn, begin_op);
}

/* #define DEBUG_PLUGIN_GEN_OPS */
static void pr_ops(void)
{
#ifdef DEBUG_PLUGIN_GEN_OPS
    TCGOp *op;
    int i = 0;

    QTAILQ_FOREACH(op, &tcg_ctx->ops, link) {
        const char *name = "";
        const char *type = "";

        if (op->opc == INDEX_op_plugin_cb_start) {
            switch (op->args[0]) {
            case PLUGIN_GEN_FROM_TB:
                name = "tb";
                break;
            case PLUGIN_GEN_FROM_INSN:
                name = "insn";
                break;
            case PLUGIN_GEN_FROM_MEM:
                name = "mem";
                break;
            case PLUGIN_GEN_AFTER_INSN:
                name = "after insn";
                break;
            default:
                break;
            }
            switch (op->args[1]) {
            case PLUGIN_GEN_CB_UDATA:
                type = "udata";
                break;
            case PLUGIN_GEN_CB_INLINE:
                type = "inline";
                break;
            case PLUGIN_GEN_CB_MEM:
                type = "mem";
                break;
            case PLUGIN_GEN_ENABLE_MEM_HELPER:
                type = "enable mem helper";
                break;
            case PLUGIN_GEN_DISABLE_MEM_HELPER:
                type = "disable mem helper";
                break;
            default:
                break;
            }
        }
        printf("op[%2i]: %s %s %s\n", i, tcg_op_defs[op->opc].name, name, type);
        i++;
    }
#endif
}

static void plugin_gen_inject(const struct qemu_plugin_tb *plugin_tb)
{
    TCGOp *op;
    int insn_idx = -1;

    pr_ops();

    QTAILQ_FOREACH(op, &tcg_ctx->ops, link) {
        switch (op->opc) {
        case INDEX_op_insn_start:
            insn_idx++;
            break;
        case INDEX_op_plugin_cb_start:
        {
            enum plugin_gen_from from = op->args[0];
            enum plugin_gen_cb type = op->args[1];

            switch (from) {
            case PLUGIN_GEN_FROM_TB:
            {
                g_assert(insn_idx == -1);

                switch (type) {
                case PLUGIN_GEN_CB_UDATA:
                    plugin_gen_tb_udata(plugin_tb, op);
                    break;
                case PLUGIN_GEN_CB_INLINE:
                    plugin_gen_tb_inline(plugin_tb, op);
                    break;
                default:
                    g_assert_not_reached();
                }
                break;
            }
            case PLUGIN_GEN_FROM_INSN:
            {
                g_assert(insn_idx >= 0);

                switch (type) {
                case PLUGIN_GEN_CB_UDATA:
                    plugin_gen_insn_udata(plugin_tb, op, insn_idx);
                    break;
                case PLUGIN_GEN_CB_INLINE:
                    plugin_gen_insn_inline(plugin_tb, op, insn_idx);
                    break;
                case PLUGIN_GEN_ENABLE_MEM_HELPER:
                    plugin_gen_enable_mem_helper(plugin_tb, op, insn_idx);
                    break;
                default:
                    g_assert_not_reached();
                }
                break;
            }
            case PLUGIN_GEN_FROM_MEM:
            {
                g_assert(insn_idx >= 0);

                switch (type) {
                case PLUGIN_GEN_CB_MEM:
                    plugin_gen_mem_regular(plugin_tb, op, insn_idx);
                    break;
                case PLUGIN_GEN_CB_INLINE:
                    plugin_gen_mem_inline(plugin_tb, op, insn_idx);
                    break;
                default:
                    g_assert_not_reached();
                }

                break;
            }
            case PLUGIN_GEN_AFTER_INSN:
            {
                g_assert(insn_idx >= 0);

                switch (type) {
                case PLUGIN_GEN_DISABLE_MEM_HELPER:
                    plugin_gen_disable_mem_helper(plugin_tb, op, insn_idx);
                    break;
                default:
                    g_assert_not_reached();
                }
                break;
            }
            default:
                g_assert_not_reached();
            }
            break;
        }
        default:
            /* plugins don't care about any other ops */
            break;
        }
    }
    pr_ops();
}

bool plugin_gen_tb_start(CPUState *cpu, const DisasContextBase *db,
                         bool mem_only)
{
    bool ret = false;

    if (test_bit(QEMU_PLUGIN_EV_VCPU_TB_TRANS, cpu->plugin_mask)) {
        struct qemu_plugin_tb *ptb = tcg_ctx->plugin_tb;
        int i;

        /* reset callbacks */
        for (i = 0; i < PLUGIN_N_CB_SUBTYPES; i++) {
            if (ptb->cbs[i]) {
                g_array_set_size(ptb->cbs[i], 0);
            }
        }
        ptb->n = 0;

        ret = true;

        ptb->vaddr = db->pc_first;
        ptb->vaddr2 = -1;
        ptb->haddr1 = db->host_addr[0];
        ptb->haddr2 = NULL;
        ptb->mem_only = mem_only;

        plugin_gen_empty_callback(PLUGIN_GEN_FROM_TB);
    }

    tcg_ctx->plugin_insn = NULL;

    return ret;
}

void plugin_gen_insn_start(CPUState *cpu, const DisasContextBase *db)
{
    struct qemu_plugin_tb *ptb = tcg_ctx->plugin_tb;
    struct qemu_plugin_insn *pinsn;

    pinsn = qemu_plugin_tb_insn_get(ptb, db->pc_next);
    tcg_ctx->plugin_insn = pinsn;
    plugin_gen_empty_callback(PLUGIN_GEN_FROM_INSN);

    /*
     * Detect page crossing to get the new host address.
     * Note that we skip this when haddr1 == NULL, e.g. when we're
     * fetching instructions from a region not backed by RAM.
     */
    if (ptb->haddr1 == NULL) {
        pinsn->haddr = NULL;
    } else if (is_same_page(db, db->pc_next)) {
        pinsn->haddr = ptb->haddr1 + pinsn->vaddr - ptb->vaddr;
    } else {
        if (ptb->vaddr2 == -1) {
            ptb->vaddr2 = TARGET_PAGE_ALIGN(db->pc_first);
            get_page_addr_code_hostp(cpu->env_ptr, ptb->vaddr2, &ptb->haddr2);
        }
        pinsn->haddr = ptb->haddr2 + pinsn->vaddr - ptb->vaddr2;
    }
}

void plugin_gen_insn_end(void)
{
    plugin_gen_empty_callback(PLUGIN_GEN_AFTER_INSN);
}

/*
 * There are cases where we never get to finalise a translation - for
 * example a page fault during translation. As a result we shouldn't
 * do any clean-up here and make sure things are reset in
 * plugin_gen_tb_start.
 */
void plugin_gen_tb_end(CPUState *cpu)
{
    struct qemu_plugin_tb *ptb = tcg_ctx->plugin_tb;

    /* collect instrumentation requests */
    qemu_plugin_tb_trans_cb(cpu, ptb);

    /* inject the instrumentation at the appropriate places */
    plugin_gen_inject(ptb);
}
