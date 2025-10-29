/*
 * vfio based device assignment support - platform devices
 *
 * Copyright Linaro Limited, 2014
 *
 * Authors:
 *  Kim Phillips <kim.phillips@linaro.org>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Based on vfio based PCI device assignment support:
 *  Copyright Red Hat, Inc. 2012
 */

#ifndef HW_VFIO_VFIO_PLATFORM_H
#define HW_VFIO_VFIO_PLATFORM_H

#include "hw/sysbus.h"
#include "hw/vfio/vfio-common.h"
#include "qemu/event_notifier.h"
#include "qemu/queue.h"
#include "qom/object.h"

#define TYPE_VFIO_PLATFORM "vfio-platform"

enum {
    VFIO_IRQ_INACTIVE = 0,
    VFIO_IRQ_PENDING = 1,
    VFIO_IRQ_ACTIVE = 2,
    /* VFIO_IRQ_ACTIVE_AND_PENDING cannot happen with VFIO */
};

typedef struct VFIOINTp {
    QLIST_ENTRY(VFIOINTp) next; /* entry for IRQ list */
    QSIMPLEQ_ENTRY(VFIOINTp) pqnext; /* entry for pending IRQ queue */
    EventNotifier *interrupt; /* eventfd triggered on interrupt */
    EventNotifier *unmask; /* eventfd for unmask on QEMU bypass */
    qemu_irq qemuirq;
    struct VFIOPlatformDevice *vdev; /* back pointer to device */
    int state; /* inactive, pending, active */
    uint8_t pin; /* index */
    uint32_t flags; /* IRQ info flags */
    bool kvm_accel; /* set when QEMU bypass through KVM enabled */
} VFIOINTp;

/* function type for user side eventfd handler */
typedef void (*eventfd_user_side_handler_t)(VFIOINTp *intp);

struct VFIOPlatformDevice {
    SysBusDevice sbdev;
    VFIODevice vbasedev; /* not a QOM object */
    VFIORegion **regions;
    QLIST_HEAD(, VFIOINTp) intp_list; /* list of IRQs */
    /* queue of pending IRQs */
    QSIMPLEQ_HEAD(, VFIOINTp) pending_intp_queue;
    char *compat; /* DT compatible values, separated by NUL */
    unsigned int num_compat; /* number of compatible values */
    uint32_t mmap_timeout; /* delay to re-enable mmaps after interrupt */
    QEMUTimer *mmap_timer; /* allows fast-path resume after IRQ hit */
    QemuMutex intp_mutex; /* protect the intp_list IRQ state */
    bool irqfd_allowed; /* debug option to force irqfd on/off */
};
typedef struct VFIOPlatformDevice VFIOPlatformDevice;

struct VFIOPlatformDeviceClass {
    /*< private >*/
    SysBusDeviceClass parent_class;
    /*< public >*/
};
typedef struct VFIOPlatformDeviceClass VFIOPlatformDeviceClass;

DECLARE_OBJ_CHECKERS(VFIOPlatformDevice, VFIOPlatformDeviceClass,
                     VFIO_PLATFORM_DEVICE, TYPE_VFIO_PLATFORM)

#endif /* HW_VFIO_VFIO_PLATFORM_H */
