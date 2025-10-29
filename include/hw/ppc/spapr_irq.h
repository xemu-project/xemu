/*
 * QEMU PowerPC sPAPR IRQ backend definitions
 *
 * Copyright (c) 2018, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later. See the
 * COPYING file in the top-level directory.
 */

#ifndef HW_SPAPR_IRQ_H
#define HW_SPAPR_IRQ_H

#include "target/ppc/cpu-qom.h"
#include "qom/object.h"

/*
 * The XIVE IRQ backend uses the same layout as the XICS backend but
 * covers the full range of the IRQ number space. The IRQ numbers for
 * the CPU IPIs are allocated at the bottom of this space, below 4K,
 * to preserve compatibility with XICS which does not use that range.
 */

/*
 * CPU IPI range (XIVE only)
 */
#define SPAPR_IRQ_IPI        0x0
#define SPAPR_IRQ_NR_IPIS    0x1000

/*
 * IRQ range offsets per device type
 */

#define SPAPR_XIRQ_BASE      XICS_IRQ_BASE /* 0x1000 */
#define SPAPR_IRQ_EPOW       (SPAPR_XIRQ_BASE + 0x0000)
#define SPAPR_IRQ_HOTPLUG    (SPAPR_XIRQ_BASE + 0x0001)
#define SPAPR_IRQ_VIO        (SPAPR_XIRQ_BASE + 0x0100)  /* 256 VIO devices */
#define SPAPR_IRQ_PCI_LSI    (SPAPR_XIRQ_BASE + 0x0200)  /* 32+ PHBs devices */

/* Offset of the dynamic range covered by the bitmap allocator */
#define SPAPR_IRQ_MSI        (SPAPR_XIRQ_BASE + 0x0300)

#define SPAPR_NR_XIRQS       0x1000

struct SpaprMachineState;

typedef struct SpaprInterruptController SpaprInterruptController;

#define TYPE_SPAPR_INTC "spapr-interrupt-controller"
#define SPAPR_INTC(obj)                                     \
    INTERFACE_CHECK(SpaprInterruptController, (obj), TYPE_SPAPR_INTC)
typedef struct SpaprInterruptControllerClass SpaprInterruptControllerClass;
DECLARE_CLASS_CHECKERS(SpaprInterruptControllerClass, SPAPR_INTC,
                       TYPE_SPAPR_INTC)

struct SpaprInterruptControllerClass {
    InterfaceClass parent;

    int (*activate)(SpaprInterruptController *intc, uint32_t nr_servers,
                    Error **errp);
    void (*deactivate)(SpaprInterruptController *intc);

    /*
     * These methods will typically be called on all intcs, active and
     * inactive
     */
    int (*cpu_intc_create)(SpaprInterruptController *intc,
                            PowerPCCPU *cpu, Error **errp);
    void (*cpu_intc_reset)(SpaprInterruptController *intc, PowerPCCPU *cpu);
    void (*cpu_intc_destroy)(SpaprInterruptController *intc, PowerPCCPU *cpu);
    int (*claim_irq)(SpaprInterruptController *intc, int irq, bool lsi,
                     Error **errp);
    void (*free_irq)(SpaprInterruptController *intc, int irq);

    /* These methods should only be called on the active intc */
    void (*set_irq)(SpaprInterruptController *intc, int irq, int val);
    void (*print_info)(SpaprInterruptController *intc, GString *buf);
    void (*dt)(SpaprInterruptController *intc, uint32_t nr_servers,
               void *fdt, uint32_t phandle);
    int (*post_load)(SpaprInterruptController *intc, int version_id);
};

void spapr_irq_update_active_intc(struct SpaprMachineState *spapr);

int spapr_irq_cpu_intc_create(struct SpaprMachineState *spapr,
                              PowerPCCPU *cpu, Error **errp);
void spapr_irq_cpu_intc_reset(struct SpaprMachineState *spapr, PowerPCCPU *cpu);
void spapr_irq_cpu_intc_destroy(struct SpaprMachineState *spapr, PowerPCCPU *cpu);
void spapr_irq_print_info(struct SpaprMachineState *spapr, GString *buf);
void spapr_irq_dt(struct SpaprMachineState *spapr, uint32_t nr_servers,
                  void *fdt, uint32_t phandle);

uint32_t spapr_irq_nr_msis(struct SpaprMachineState *spapr);
int spapr_irq_msi_alloc(struct SpaprMachineState *spapr, uint32_t num, bool align,
                        Error **errp);
void spapr_irq_msi_free(struct SpaprMachineState *spapr, int irq, uint32_t num);

typedef struct SpaprIrq {
    bool        xics;
    bool        xive;
} SpaprIrq;

extern SpaprIrq spapr_irq_xics;
extern SpaprIrq spapr_irq_xics_legacy;
extern SpaprIrq spapr_irq_xive;
extern SpaprIrq spapr_irq_dual;

void spapr_irq_init(struct SpaprMachineState *spapr, Error **errp);
int spapr_irq_claim(struct SpaprMachineState *spapr, int irq, bool lsi, Error **errp);
void spapr_irq_free(struct SpaprMachineState *spapr, int irq, int num);
qemu_irq spapr_qirq(struct SpaprMachineState *spapr, int irq);
int spapr_irq_post_load(struct SpaprMachineState *spapr, int version_id);
void spapr_irq_reset(struct SpaprMachineState *spapr, Error **errp);
int spapr_irq_get_phandle(struct SpaprMachineState *spapr, void *fdt, Error **errp);

typedef int (*SpaprInterruptControllerInitKvm)(SpaprInterruptController *,
                                               uint32_t, Error **);

int spapr_irq_init_kvm(SpaprInterruptControllerInitKvm fn,
                       SpaprInterruptController *intc,
                       uint32_t nr_servers,
                       Error **errp);

/*
 * XICS legacy routines
 */
int spapr_irq_find(struct SpaprMachineState *spapr, int num, bool align, Error **errp);
#define spapr_irq_findone(spapr, errp) spapr_irq_find(spapr, 1, false, errp)

#endif
