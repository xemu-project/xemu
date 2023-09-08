/*
 * Copyright (C) 2014-2016 Broadcom Corporation
 * Copyright (c) 2017 Red Hat, Inc.
 * Written by Prem Mallappa, Eric Auger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Prem Mallappa <pmallapp@broadcom.com>
 *
 */

#include "qemu/osdep.h"
#include "trace.h"
#include "exec/target_page.h"
#include "hw/core/cpu.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/jhash.h"
#include "qemu/module.h"

#include "qemu/error-report.h"
#include "hw/arm/smmu-common.h"
#include "smmu-internal.h"

/* IOTLB Management */

static guint smmu_iotlb_key_hash(gconstpointer v)
{
    SMMUIOTLBKey *key = (SMMUIOTLBKey *)v;
    uint32_t a, b, c;

    /* Jenkins hash */
    a = b = c = JHASH_INITVAL + sizeof(*key);
    a += key->asid + key->level + key->tg;
    b += extract64(key->iova, 0, 32);
    c += extract64(key->iova, 32, 32);

    __jhash_mix(a, b, c);
    __jhash_final(a, b, c);

    return c;
}

static gboolean smmu_iotlb_key_equal(gconstpointer v1, gconstpointer v2)
{
    SMMUIOTLBKey *k1 = (SMMUIOTLBKey *)v1, *k2 = (SMMUIOTLBKey *)v2;

    return (k1->asid == k2->asid) && (k1->iova == k2->iova) &&
           (k1->level == k2->level) && (k1->tg == k2->tg);
}

SMMUIOTLBKey smmu_get_iotlb_key(uint16_t asid, uint64_t iova,
                                uint8_t tg, uint8_t level)
{
    SMMUIOTLBKey key = {.asid = asid, .iova = iova, .tg = tg, .level = level};

    return key;
}

SMMUTLBEntry *smmu_iotlb_lookup(SMMUState *bs, SMMUTransCfg *cfg,
                                SMMUTransTableInfo *tt, hwaddr iova)
{
    uint8_t tg = (tt->granule_sz - 10) / 2;
    uint8_t inputsize = 64 - tt->tsz;
    uint8_t stride = tt->granule_sz - 3;
    uint8_t level = 4 - (inputsize - 4) / stride;
    SMMUTLBEntry *entry = NULL;

    while (level <= 3) {
        uint64_t subpage_size = 1ULL << level_shift(level, tt->granule_sz);
        uint64_t mask = subpage_size - 1;
        SMMUIOTLBKey key;

        key = smmu_get_iotlb_key(cfg->asid, iova & ~mask, tg, level);
        entry = g_hash_table_lookup(bs->iotlb, &key);
        if (entry) {
            break;
        }
        level++;
    }

    if (entry) {
        cfg->iotlb_hits++;
        trace_smmu_iotlb_lookup_hit(cfg->asid, iova,
                                    cfg->iotlb_hits, cfg->iotlb_misses,
                                    100 * cfg->iotlb_hits /
                                    (cfg->iotlb_hits + cfg->iotlb_misses));
    } else {
        cfg->iotlb_misses++;
        trace_smmu_iotlb_lookup_miss(cfg->asid, iova,
                                     cfg->iotlb_hits, cfg->iotlb_misses,
                                     100 * cfg->iotlb_hits /
                                     (cfg->iotlb_hits + cfg->iotlb_misses));
    }
    return entry;
}

void smmu_iotlb_insert(SMMUState *bs, SMMUTransCfg *cfg, SMMUTLBEntry *new)
{
    SMMUIOTLBKey *key = g_new0(SMMUIOTLBKey, 1);
    uint8_t tg = (new->granule - 10) / 2;

    if (g_hash_table_size(bs->iotlb) >= SMMU_IOTLB_MAX_SIZE) {
        smmu_iotlb_inv_all(bs);
    }

    *key = smmu_get_iotlb_key(cfg->asid, new->entry.iova, tg, new->level);
    trace_smmu_iotlb_insert(cfg->asid, new->entry.iova, tg, new->level);
    g_hash_table_insert(bs->iotlb, key, new);
}

inline void smmu_iotlb_inv_all(SMMUState *s)
{
    trace_smmu_iotlb_inv_all();
    g_hash_table_remove_all(s->iotlb);
}

static gboolean smmu_hash_remove_by_asid(gpointer key, gpointer value,
                                         gpointer user_data)
{
    uint16_t asid = *(uint16_t *)user_data;
    SMMUIOTLBKey *iotlb_key = (SMMUIOTLBKey *)key;

    return SMMU_IOTLB_ASID(*iotlb_key) == asid;
}

static gboolean smmu_hash_remove_by_asid_iova(gpointer key, gpointer value,
                                              gpointer user_data)
{
    SMMUTLBEntry *iter = (SMMUTLBEntry *)value;
    IOMMUTLBEntry *entry = &iter->entry;
    SMMUIOTLBPageInvInfo *info = (SMMUIOTLBPageInvInfo *)user_data;
    SMMUIOTLBKey iotlb_key = *(SMMUIOTLBKey *)key;

    if (info->asid >= 0 && info->asid != SMMU_IOTLB_ASID(iotlb_key)) {
        return false;
    }
    return ((info->iova & ~entry->addr_mask) == entry->iova) ||
           ((entry->iova & ~info->mask) == info->iova);
}

inline void
smmu_iotlb_inv_iova(SMMUState *s, int asid, dma_addr_t iova,
                    uint8_t tg, uint64_t num_pages, uint8_t ttl)
{
    /* if tg is not set we use 4KB range invalidation */
    uint8_t granule = tg ? tg * 2 + 10 : 12;

    if (ttl && (num_pages == 1) && (asid >= 0)) {
        SMMUIOTLBKey key = smmu_get_iotlb_key(asid, iova, tg, ttl);

        if (g_hash_table_remove(s->iotlb, &key)) {
            return;
        }
        /*
         * if the entry is not found, let's see if it does not
         * belong to a larger IOTLB entry
         */
    }

    SMMUIOTLBPageInvInfo info = {
        .asid = asid, .iova = iova,
        .mask = (num_pages * 1 << granule) - 1};

    g_hash_table_foreach_remove(s->iotlb,
                                smmu_hash_remove_by_asid_iova,
                                &info);
}

inline void smmu_iotlb_inv_asid(SMMUState *s, uint16_t asid)
{
    trace_smmu_iotlb_inv_asid(asid);
    g_hash_table_foreach_remove(s->iotlb, smmu_hash_remove_by_asid, &asid);
}

/* VMSAv8-64 Translation */

/**
 * get_pte - Get the content of a page table entry located at
 * @base_addr[@index]
 */
static int get_pte(dma_addr_t baseaddr, uint32_t index, uint64_t *pte,
                   SMMUPTWEventInfo *info)
{
    int ret;
    dma_addr_t addr = baseaddr + index * sizeof(*pte);

    /* TODO: guarantee 64-bit single-copy atomicity */
    ret = dma_memory_read(&address_space_memory, addr, pte, sizeof(*pte),
                          MEMTXATTRS_UNSPECIFIED);

    if (ret != MEMTX_OK) {
        info->type = SMMU_PTW_ERR_WALK_EABT;
        info->addr = addr;
        return -EINVAL;
    }
    trace_smmu_get_pte(baseaddr, index, addr, *pte);
    return 0;
}

/* VMSAv8-64 Translation Table Format Descriptor Decoding */

/**
 * get_page_pte_address - returns the L3 descriptor output address,
 * ie. the page frame
 * ARM ARM spec: Figure D4-17 VMSAv8-64 level 3 descriptor format
 */
static inline hwaddr get_page_pte_address(uint64_t pte, int granule_sz)
{
    return PTE_ADDRESS(pte, granule_sz);
}

/**
 * get_table_pte_address - return table descriptor output address,
 * ie. address of next level table
 * ARM ARM Figure D4-16 VMSAv8-64 level0, level1, and level 2 descriptor formats
 */
static inline hwaddr get_table_pte_address(uint64_t pte, int granule_sz)
{
    return PTE_ADDRESS(pte, granule_sz);
}

/**
 * get_block_pte_address - return block descriptor output address and block size
 * ARM ARM Figure D4-16 VMSAv8-64 level0, level1, and level 2 descriptor formats
 */
static inline hwaddr get_block_pte_address(uint64_t pte, int level,
                                           int granule_sz, uint64_t *bsz)
{
    int n = level_shift(level, granule_sz);

    *bsz = 1ULL << n;
    return PTE_ADDRESS(pte, n);
}

SMMUTransTableInfo *select_tt(SMMUTransCfg *cfg, dma_addr_t iova)
{
    bool tbi = extract64(iova, 55, 1) ? TBI1(cfg->tbi) : TBI0(cfg->tbi);
    uint8_t tbi_byte = tbi * 8;

    if (cfg->tt[0].tsz &&
        !extract64(iova, 64 - cfg->tt[0].tsz, cfg->tt[0].tsz - tbi_byte)) {
        /* there is a ttbr0 region and we are in it (high bits all zero) */
        return &cfg->tt[0];
    } else if (cfg->tt[1].tsz &&
           !extract64(iova, 64 - cfg->tt[1].tsz, cfg->tt[1].tsz - tbi_byte)) {
        /* there is a ttbr1 region and we are in it (high bits all one) */
        return &cfg->tt[1];
    } else if (!cfg->tt[0].tsz) {
        /* ttbr0 region is "everything not in the ttbr1 region" */
        return &cfg->tt[0];
    } else if (!cfg->tt[1].tsz) {
        /* ttbr1 region is "everything not in the ttbr0 region" */
        return &cfg->tt[1];
    }
    /* in the gap between the two regions, this is a Translation fault */
    return NULL;
}

/**
 * smmu_ptw_64 - VMSAv8-64 Walk of the page tables for a given IOVA
 * @cfg: translation config
 * @iova: iova to translate
 * @perm: access type
 * @tlbe: SMMUTLBEntry (out)
 * @info: handle to an error info
 *
 * Return 0 on success, < 0 on error. In case of error, @info is filled
 * and tlbe->perm is set to IOMMU_NONE.
 * Upon success, @tlbe is filled with translated_addr and entry
 * permission rights.
 */
static int smmu_ptw_64(SMMUTransCfg *cfg,
                       dma_addr_t iova, IOMMUAccessFlags perm,
                       SMMUTLBEntry *tlbe, SMMUPTWEventInfo *info)
{
    dma_addr_t baseaddr, indexmask;
    int stage = cfg->stage;
    SMMUTransTableInfo *tt = select_tt(cfg, iova);
    uint8_t level, granule_sz, inputsize, stride;

    if (!tt || tt->disabled) {
        info->type = SMMU_PTW_ERR_TRANSLATION;
        goto error;
    }

    granule_sz = tt->granule_sz;
    stride = granule_sz - 3;
    inputsize = 64 - tt->tsz;
    level = 4 - (inputsize - 4) / stride;
    indexmask = (1ULL << (inputsize - (stride * (4 - level)))) - 1;
    baseaddr = extract64(tt->ttb, 0, 48);
    baseaddr &= ~indexmask;

    while (level <= 3) {
        uint64_t subpage_size = 1ULL << level_shift(level, granule_sz);
        uint64_t mask = subpage_size - 1;
        uint32_t offset = iova_level_offset(iova, inputsize, level, granule_sz);
        uint64_t pte, gpa;
        dma_addr_t pte_addr = baseaddr + offset * sizeof(pte);
        uint8_t ap;

        if (get_pte(baseaddr, offset, &pte, info)) {
                goto error;
        }
        trace_smmu_ptw_level(level, iova, subpage_size,
                             baseaddr, offset, pte);

        if (is_invalid_pte(pte) || is_reserved_pte(pte, level)) {
            trace_smmu_ptw_invalid_pte(stage, level, baseaddr,
                                       pte_addr, offset, pte);
            break;
        }

        if (is_table_pte(pte, level)) {
            ap = PTE_APTABLE(pte);

            if (is_permission_fault(ap, perm) && !tt->had) {
                info->type = SMMU_PTW_ERR_PERMISSION;
                goto error;
            }
            baseaddr = get_table_pte_address(pte, granule_sz);
            level++;
            continue;
        } else if (is_page_pte(pte, level)) {
            gpa = get_page_pte_address(pte, granule_sz);
            trace_smmu_ptw_page_pte(stage, level, iova,
                                    baseaddr, pte_addr, pte, gpa);
        } else {
            uint64_t block_size;

            gpa = get_block_pte_address(pte, level, granule_sz,
                                        &block_size);
            trace_smmu_ptw_block_pte(stage, level, baseaddr,
                                     pte_addr, pte, iova, gpa,
                                     block_size >> 20);
        }
        ap = PTE_AP(pte);
        if (is_permission_fault(ap, perm)) {
            info->type = SMMU_PTW_ERR_PERMISSION;
            goto error;
        }

        tlbe->entry.translated_addr = gpa;
        tlbe->entry.iova = iova & ~mask;
        tlbe->entry.addr_mask = mask;
        tlbe->entry.perm = PTE_AP_TO_PERM(ap);
        tlbe->level = level;
        tlbe->granule = granule_sz;
        return 0;
    }
    info->type = SMMU_PTW_ERR_TRANSLATION;

error:
    tlbe->entry.perm = IOMMU_NONE;
    return -EINVAL;
}

/**
 * smmu_ptw - Walk the page tables for an IOVA, according to @cfg
 *
 * @cfg: translation configuration
 * @iova: iova to translate
 * @perm: tentative access type
 * @tlbe: returned entry
 * @info: ptw event handle
 *
 * return 0 on success
 */
inline int smmu_ptw(SMMUTransCfg *cfg, dma_addr_t iova, IOMMUAccessFlags perm,
                    SMMUTLBEntry *tlbe, SMMUPTWEventInfo *info)
{
    if (!cfg->aa64) {
        /*
         * This code path is not entered as we check this while decoding
         * the configuration data in the derived SMMU model.
         */
        g_assert_not_reached();
    }

    return smmu_ptw_64(cfg, iova, perm, tlbe, info);
}

/**
 * The bus number is used for lookup when SID based invalidation occurs.
 * In that case we lazily populate the SMMUPciBus array from the bus hash
 * table. At the time the SMMUPciBus is created (smmu_find_add_as), the bus
 * numbers may not be always initialized yet.
 */
SMMUPciBus *smmu_find_smmu_pcibus(SMMUState *s, uint8_t bus_num)
{
    SMMUPciBus *smmu_pci_bus = s->smmu_pcibus_by_bus_num[bus_num];
    GHashTableIter iter;

    if (smmu_pci_bus) {
        return smmu_pci_bus;
    }

    g_hash_table_iter_init(&iter, s->smmu_pcibus_by_busptr);
    while (g_hash_table_iter_next(&iter, NULL, (void **)&smmu_pci_bus)) {
        if (pci_bus_num(smmu_pci_bus->bus) == bus_num) {
            s->smmu_pcibus_by_bus_num[bus_num] = smmu_pci_bus;
            return smmu_pci_bus;
        }
    }

    return NULL;
}

static AddressSpace *smmu_find_add_as(PCIBus *bus, void *opaque, int devfn)
{
    SMMUState *s = opaque;
    SMMUPciBus *sbus = g_hash_table_lookup(s->smmu_pcibus_by_busptr, bus);
    SMMUDevice *sdev;
    static unsigned int index;

    if (!sbus) {
        sbus = g_malloc0(sizeof(SMMUPciBus) +
                         sizeof(SMMUDevice *) * SMMU_PCI_DEVFN_MAX);
        sbus->bus = bus;
        g_hash_table_insert(s->smmu_pcibus_by_busptr, bus, sbus);
    }

    sdev = sbus->pbdev[devfn];
    if (!sdev) {
        char *name = g_strdup_printf("%s-%d-%d", s->mrtypename, devfn, index++);

        sdev = sbus->pbdev[devfn] = g_new0(SMMUDevice, 1);

        sdev->smmu = s;
        sdev->bus = bus;
        sdev->devfn = devfn;

        memory_region_init_iommu(&sdev->iommu, sizeof(sdev->iommu),
                                 s->mrtypename,
                                 OBJECT(s), name, 1ULL << SMMU_MAX_VA_BITS);
        address_space_init(&sdev->as,
                           MEMORY_REGION(&sdev->iommu), name);
        trace_smmu_add_mr(name);
        g_free(name);
    }

    return &sdev->as;
}

IOMMUMemoryRegion *smmu_iommu_mr(SMMUState *s, uint32_t sid)
{
    uint8_t bus_n, devfn;
    SMMUPciBus *smmu_bus;
    SMMUDevice *smmu;

    bus_n = PCI_BUS_NUM(sid);
    smmu_bus = smmu_find_smmu_pcibus(s, bus_n);
    if (smmu_bus) {
        devfn = SMMU_PCI_DEVFN(sid);
        smmu = smmu_bus->pbdev[devfn];
        if (smmu) {
            return &smmu->iommu;
        }
    }
    return NULL;
}

/* Unmap the whole notifier's range */
static void smmu_unmap_notifier_range(IOMMUNotifier *n)
{
    IOMMUTLBEvent event;

    event.type = IOMMU_NOTIFIER_UNMAP;
    event.entry.target_as = &address_space_memory;
    event.entry.iova = n->start;
    event.entry.perm = IOMMU_NONE;
    event.entry.addr_mask = n->end - n->start;

    memory_region_notify_iommu_one(n, &event);
}

/* Unmap all notifiers attached to @mr */
inline void smmu_inv_notifiers_mr(IOMMUMemoryRegion *mr)
{
    IOMMUNotifier *n;

    trace_smmu_inv_notifiers_mr(mr->parent_obj.name);
    IOMMU_NOTIFIER_FOREACH(n, mr) {
        smmu_unmap_notifier_range(n);
    }
}

/* Unmap all notifiers of all mr's */
void smmu_inv_notifiers_all(SMMUState *s)
{
    SMMUDevice *sdev;

    QLIST_FOREACH(sdev, &s->devices_with_notifiers, next) {
        smmu_inv_notifiers_mr(&sdev->iommu);
    }
}

static void smmu_base_realize(DeviceState *dev, Error **errp)
{
    SMMUState *s = ARM_SMMU(dev);
    SMMUBaseClass *sbc = ARM_SMMU_GET_CLASS(dev);
    Error *local_err = NULL;

    sbc->parent_realize(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }
    s->configs = g_hash_table_new_full(NULL, NULL, NULL, g_free);
    s->iotlb = g_hash_table_new_full(smmu_iotlb_key_hash, smmu_iotlb_key_equal,
                                     g_free, g_free);
    s->smmu_pcibus_by_busptr = g_hash_table_new(NULL, NULL);

    if (s->primary_bus) {
        pci_setup_iommu(s->primary_bus, smmu_find_add_as, s);
    } else {
        error_setg(errp, "SMMU is not attached to any PCI bus!");
    }
}

static void smmu_base_reset(DeviceState *dev)
{
    SMMUState *s = ARM_SMMU(dev);

    g_hash_table_remove_all(s->configs);
    g_hash_table_remove_all(s->iotlb);
}

static Property smmu_dev_properties[] = {
    DEFINE_PROP_UINT8("bus_num", SMMUState, bus_num, 0),
    DEFINE_PROP_LINK("primary-bus", SMMUState, primary_bus, "PCI", PCIBus *),
    DEFINE_PROP_END_OF_LIST(),
};

static void smmu_base_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMMUBaseClass *sbc = ARM_SMMU_CLASS(klass);

    device_class_set_props(dc, smmu_dev_properties);
    device_class_set_parent_realize(dc, smmu_base_realize,
                                    &sbc->parent_realize);
    dc->reset = smmu_base_reset;
}

static const TypeInfo smmu_base_info = {
    .name          = TYPE_ARM_SMMU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SMMUState),
    .class_data    = NULL,
    .class_size    = sizeof(SMMUBaseClass),
    .class_init    = smmu_base_class_init,
    .abstract      = true,
};

static void smmu_base_register_types(void)
{
    type_register_static(&smmu_base_info);
}

type_init(smmu_base_register_types)

