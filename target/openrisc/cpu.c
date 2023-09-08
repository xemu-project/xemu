/*
 * QEMU OpenRISC CPU
 *
 * Copyright (c) 2012 Jia Liu <proljc@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "exec/exec-all.h"

static void openrisc_cpu_set_pc(CPUState *cs, vaddr value)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);

    cpu->env.pc = value;
    cpu->env.dflag = 0;
}

static vaddr openrisc_cpu_get_pc(CPUState *cs)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);

    return cpu->env.pc;
}

static void openrisc_cpu_synchronize_from_tb(CPUState *cs,
                                             const TranslationBlock *tb)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);

    cpu->env.pc = tb_pc(tb);
}

static void openrisc_restore_state_to_opc(CPUState *cs,
                                          const TranslationBlock *tb,
                                          const uint64_t *data)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(cs);

    cpu->env.pc = data[0];
    cpu->env.dflag = data[1] & 1;
    if (data[1] & 2) {
        cpu->env.ppc = cpu->env.pc - 4;
    }
}

static bool openrisc_cpu_has_work(CPUState *cs)
{
    return cs->interrupt_request & (CPU_INTERRUPT_HARD |
                                    CPU_INTERRUPT_TIMER);
}

static void openrisc_disas_set_info(CPUState *cpu, disassemble_info *info)
{
    info->print_insn = print_insn_or1k;
}

static void openrisc_cpu_reset(DeviceState *dev)
{
    CPUState *s = CPU(dev);
    OpenRISCCPU *cpu = OPENRISC_CPU(s);
    OpenRISCCPUClass *occ = OPENRISC_CPU_GET_CLASS(cpu);

    occ->parent_reset(dev);

    memset(&cpu->env, 0, offsetof(CPUOpenRISCState, end_reset_fields));

    cpu->env.pc = 0x100;
    cpu->env.sr = SR_FO | SR_SM;
    cpu->env.lock_addr = -1;
    s->exception_index = -1;
    cpu_set_fpcsr(&cpu->env, 0);

#ifndef CONFIG_USER_ONLY
    cpu->env.picmr = 0x00000000;
    cpu->env.picsr = 0x00000000;

    cpu->env.ttmr = 0x00000000;
#endif
}

#ifndef CONFIG_USER_ONLY
static void openrisc_cpu_set_irq(void *opaque, int irq, int level)
{
    OpenRISCCPU *cpu = (OpenRISCCPU *)opaque;
    CPUState *cs = CPU(cpu);
    uint32_t irq_bit;

    if (irq > 31 || irq < 0) {
        return;
    }

    irq_bit = 1U << irq;

    if (level) {
        cpu->env.picsr |= irq_bit;
    } else {
        cpu->env.picsr &= ~irq_bit;
    }

    if (cpu->env.picsr & cpu->env.picmr) {
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
    }
}
#endif

static void openrisc_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    OpenRISCCPUClass *occ = OPENRISC_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

    qemu_init_vcpu(cs);
    cpu_reset(cs);

    occ->parent_realize(dev, errp);
}

static void openrisc_cpu_initfn(Object *obj)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(obj);

    cpu_set_cpustate_pointers(cpu);

#ifndef CONFIG_USER_ONLY
    qdev_init_gpio_in_named(DEVICE(cpu), openrisc_cpu_set_irq, "IRQ", NR_IRQS);
#endif
}

/* CPU models */

static ObjectClass *openrisc_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;

    typename = g_strdup_printf(OPENRISC_CPU_TYPE_NAME("%s"), cpu_model);
    oc = object_class_by_name(typename);
    g_free(typename);
    if (oc != NULL && (!object_class_dynamic_cast(oc, TYPE_OPENRISC_CPU) ||
                       object_class_is_abstract(oc))) {
        return NULL;
    }
    return oc;
}

static void or1200_initfn(Object *obj)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(obj);

    cpu->env.vr = 0x13000008;
    cpu->env.upr = UPR_UP | UPR_DMP | UPR_IMP | UPR_PICP | UPR_TTP | UPR_PMP;
    cpu->env.cpucfgr = CPUCFGR_NSGF | CPUCFGR_OB32S | CPUCFGR_OF32S |
                       CPUCFGR_EVBARP;

    /* 1Way, TLB_SIZE entries.  */
    cpu->env.dmmucfgr = (DMMUCFGR_NTW & (0 << 2))
                      | (DMMUCFGR_NTS & (ctz32(TLB_SIZE) << 2));
    cpu->env.immucfgr = (IMMUCFGR_NTW & (0 << 2))
                      | (IMMUCFGR_NTS & (ctz32(TLB_SIZE) << 2));
}

static void openrisc_any_initfn(Object *obj)
{
    OpenRISCCPU *cpu = OPENRISC_CPU(obj);

    cpu->env.vr = 0x13000040;   /* Obsolete VER + UVRP for new SPRs */
    cpu->env.vr2 = 0;           /* No version specific id */
    cpu->env.avr = 0x01030000;  /* Architecture v1.3 */

    cpu->env.upr = UPR_UP | UPR_DMP | UPR_IMP | UPR_PICP | UPR_TTP | UPR_PMP;
    cpu->env.cpucfgr = CPUCFGR_NSGF | CPUCFGR_OB32S | CPUCFGR_OF32S |
                       CPUCFGR_AVRP | CPUCFGR_EVBARP | CPUCFGR_OF64A32S;

    /* 1Way, TLB_SIZE entries.  */
    cpu->env.dmmucfgr = (DMMUCFGR_NTW & (0 << 2))
                      | (DMMUCFGR_NTS & (ctz32(TLB_SIZE) << 2));
    cpu->env.immucfgr = (IMMUCFGR_NTW & (0 << 2))
                      | (IMMUCFGR_NTS & (ctz32(TLB_SIZE) << 2));
}

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps openrisc_sysemu_ops = {
    .get_phys_page_debug = openrisc_cpu_get_phys_page_debug,
};
#endif

#include "hw/core/tcg-cpu-ops.h"

static const struct TCGCPUOps openrisc_tcg_ops = {
    .initialize = openrisc_translate_init,
    .synchronize_from_tb = openrisc_cpu_synchronize_from_tb,
    .restore_state_to_opc = openrisc_restore_state_to_opc,

#ifndef CONFIG_USER_ONLY
    .tlb_fill = openrisc_cpu_tlb_fill,
    .cpu_exec_interrupt = openrisc_cpu_exec_interrupt,
    .do_interrupt = openrisc_cpu_do_interrupt,
#endif /* !CONFIG_USER_ONLY */
};

static void openrisc_cpu_class_init(ObjectClass *oc, void *data)
{
    OpenRISCCPUClass *occ = OPENRISC_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(occ);
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_parent_realize(dc, openrisc_cpu_realizefn,
                                    &occ->parent_realize);
    device_class_set_parent_reset(dc, openrisc_cpu_reset, &occ->parent_reset);

    cc->class_by_name = openrisc_cpu_class_by_name;
    cc->has_work = openrisc_cpu_has_work;
    cc->dump_state = openrisc_cpu_dump_state;
    cc->set_pc = openrisc_cpu_set_pc;
    cc->get_pc = openrisc_cpu_get_pc;
    cc->gdb_read_register = openrisc_cpu_gdb_read_register;
    cc->gdb_write_register = openrisc_cpu_gdb_write_register;
#ifndef CONFIG_USER_ONLY
    dc->vmsd = &vmstate_openrisc_cpu;
    cc->sysemu_ops = &openrisc_sysemu_ops;
#endif
    cc->gdb_num_core_regs = 32 + 3;
    cc->disas_set_info = openrisc_disas_set_info;
    cc->tcg_ops = &openrisc_tcg_ops;
}

/* Sort alphabetically by type name, except for "any". */
static gint openrisc_cpu_list_compare(gconstpointer a, gconstpointer b)
{
    ObjectClass *class_a = (ObjectClass *)a;
    ObjectClass *class_b = (ObjectClass *)b;
    const char *name_a, *name_b;

    name_a = object_class_get_name(class_a);
    name_b = object_class_get_name(class_b);
    if (strcmp(name_a, "any-" TYPE_OPENRISC_CPU) == 0) {
        return 1;
    } else if (strcmp(name_b, "any-" TYPE_OPENRISC_CPU) == 0) {
        return -1;
    } else {
        return strcmp(name_a, name_b);
    }
}

static void openrisc_cpu_list_entry(gpointer data, gpointer user_data)
{
    ObjectClass *oc = data;
    const char *typename;
    char *name;

    typename = object_class_get_name(oc);
    name = g_strndup(typename,
                     strlen(typename) - strlen("-" TYPE_OPENRISC_CPU));
    qemu_printf("  %s\n", name);
    g_free(name);
}

void cpu_openrisc_list(void)
{
    GSList *list;

    list = object_class_get_list(TYPE_OPENRISC_CPU, false);
    list = g_slist_sort(list, openrisc_cpu_list_compare);
    qemu_printf("Available CPUs:\n");
    g_slist_foreach(list, openrisc_cpu_list_entry, NULL);
    g_slist_free(list);
}

#define DEFINE_OPENRISC_CPU_TYPE(cpu_model, initfn) \
    {                                               \
        .parent = TYPE_OPENRISC_CPU,                \
        .instance_init = initfn,                    \
        .name = OPENRISC_CPU_TYPE_NAME(cpu_model),  \
    }

static const TypeInfo openrisc_cpus_type_infos[] = {
    { /* base class should be registered first */
        .name = TYPE_OPENRISC_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(OpenRISCCPU),
        .instance_init = openrisc_cpu_initfn,
        .abstract = true,
        .class_size = sizeof(OpenRISCCPUClass),
        .class_init = openrisc_cpu_class_init,
    },
    DEFINE_OPENRISC_CPU_TYPE("or1200", or1200_initfn),
    DEFINE_OPENRISC_CPU_TYPE("any", openrisc_any_initfn),
};

DEFINE_TYPES(openrisc_cpus_type_infos)
