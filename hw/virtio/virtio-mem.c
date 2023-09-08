/*
 * Virtio MEM device
 *
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * Authors:
 *  David Hildenbrand <david@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qemu/units.h"
#include "sysemu/numa.h"
#include "sysemu/sysemu.h"
#include "sysemu/reset.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-access.h"
#include "hw/virtio/virtio-mem.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "exec/ram_addr.h"
#include "migration/misc.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include CONFIG_DEVICES
#include "trace.h"

/*
 * We only had legacy x86 guests that did not support
 * VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE. Other targets don't have legacy guests.
 */
#if defined(TARGET_X86_64) || defined(TARGET_I386)
#define VIRTIO_MEM_HAS_LEGACY_GUESTS
#endif

/*
 * Let's not allow blocks smaller than 1 MiB, for example, to keep the tracking
 * bitmap small.
 */
#define VIRTIO_MEM_MIN_BLOCK_SIZE ((uint32_t)(1 * MiB))

static uint32_t virtio_mem_default_thp_size(void)
{
    uint32_t default_thp_size = VIRTIO_MEM_MIN_BLOCK_SIZE;

#if defined(__x86_64__) || defined(__arm__) || defined(__powerpc64__)
    default_thp_size = 2 * MiB;
#elif defined(__aarch64__)
    if (qemu_real_host_page_size() == 4 * KiB) {
        default_thp_size = 2 * MiB;
    } else if (qemu_real_host_page_size() == 16 * KiB) {
        default_thp_size = 32 * MiB;
    } else if (qemu_real_host_page_size() == 64 * KiB) {
        default_thp_size = 512 * MiB;
    }
#endif

    return default_thp_size;
}

/*
 * We want to have a reasonable default block size such that
 * 1. We avoid splitting THPs when unplugging memory, which degrades
 *    performance.
 * 2. We avoid placing THPs for plugged blocks that also cover unplugged
 *    blocks.
 *
 * The actual THP size might differ between Linux kernels, so we try to probe
 * it. In the future (if we ever run into issues regarding 2.), we might want
 * to disable THP in case we fail to properly probe the THP size, or if the
 * block size is configured smaller than the THP size.
 */
static uint32_t thp_size;

#define HPAGE_PMD_SIZE_PATH "/sys/kernel/mm/transparent_hugepage/hpage_pmd_size"
static uint32_t virtio_mem_thp_size(void)
{
    gchar *content = NULL;
    const char *endptr;
    uint64_t tmp;

    if (thp_size) {
        return thp_size;
    }

    /*
     * Try to probe the actual THP size, fallback to (sane but eventually
     * incorrect) default sizes.
     */
    if (g_file_get_contents(HPAGE_PMD_SIZE_PATH, &content, NULL, NULL) &&
        !qemu_strtou64(content, &endptr, 0, &tmp) &&
        (!endptr || *endptr == '\n')) {
        /* Sanity-check the value and fallback to something reasonable. */
        if (!tmp || !is_power_of_2(tmp)) {
            warn_report("Read unsupported THP size: %" PRIx64, tmp);
        } else {
            thp_size = tmp;
        }
    }

    if (!thp_size) {
        thp_size = virtio_mem_default_thp_size();
        warn_report("Could not detect THP size, falling back to %" PRIx64
                    "  MiB.", thp_size / MiB);
    }

    g_free(content);
    return thp_size;
}

static uint64_t virtio_mem_default_block_size(RAMBlock *rb)
{
    const uint64_t page_size = qemu_ram_pagesize(rb);

    /* We can have hugetlbfs with a page size smaller than the THP size. */
    if (page_size == qemu_real_host_page_size()) {
        return MAX(page_size, virtio_mem_thp_size());
    }
    return MAX(page_size, VIRTIO_MEM_MIN_BLOCK_SIZE);
}

#if defined(VIRTIO_MEM_HAS_LEGACY_GUESTS)
static bool virtio_mem_has_shared_zeropage(RAMBlock *rb)
{
    /*
     * We only have a guaranteed shared zeropage on ordinary MAP_PRIVATE
     * anonymous RAM. In any other case, reading unplugged *can* populate a
     * fresh page, consuming actual memory.
     */
    return !qemu_ram_is_shared(rb) && rb->fd < 0 &&
           qemu_ram_pagesize(rb) == qemu_real_host_page_size();
}
#endif /* VIRTIO_MEM_HAS_LEGACY_GUESTS */

/*
 * Size the usable region bigger than the requested size if possible. Esp.
 * Linux guests will only add (aligned) memory blocks in case they fully
 * fit into the usable region, but plug+online only a subset of the pages.
 * The memory block size corresponds mostly to the section size.
 *
 * This allows e.g., to add 20MB with a section size of 128MB on x86_64, and
 * a section size of 512MB on arm64 (as long as the start address is properly
 * aligned, similar to ordinary DIMMs).
 *
 * We can change this at any time and maybe even make it configurable if
 * necessary (as the section size can change). But it's more likely that the
 * section size will rather get smaller and not bigger over time.
 */
#if defined(TARGET_X86_64) || defined(TARGET_I386)
#define VIRTIO_MEM_USABLE_EXTENT (2 * (128 * MiB))
#elif defined(TARGET_ARM)
#define VIRTIO_MEM_USABLE_EXTENT (2 * (512 * MiB))
#else
#error VIRTIO_MEM_USABLE_EXTENT not defined
#endif

static bool virtio_mem_is_busy(void)
{
    /*
     * Postcopy cannot handle concurrent discards and we don't want to migrate
     * pages on-demand with stale content when plugging new blocks.
     *
     * For precopy, we don't want unplugged blocks in our migration stream, and
     * when plugging new blocks, the page content might differ between source
     * and destination (observable by the guest when not initializing pages
     * after plugging them) until we're running on the destination (as we didn't
     * migrate these blocks when they were unplugged).
     */
    return migration_in_incoming_postcopy() || !migration_is_idle();
}

typedef int (*virtio_mem_range_cb)(const VirtIOMEM *vmem, void *arg,
                                   uint64_t offset, uint64_t size);

static int virtio_mem_for_each_unplugged_range(const VirtIOMEM *vmem, void *arg,
                                               virtio_mem_range_cb cb)
{
    unsigned long first_zero_bit, last_zero_bit;
    uint64_t offset, size;
    int ret = 0;

    first_zero_bit = find_first_zero_bit(vmem->bitmap, vmem->bitmap_size);
    while (first_zero_bit < vmem->bitmap_size) {
        offset = first_zero_bit * vmem->block_size;
        last_zero_bit = find_next_bit(vmem->bitmap, vmem->bitmap_size,
                                      first_zero_bit + 1) - 1;
        size = (last_zero_bit - first_zero_bit + 1) * vmem->block_size;

        ret = cb(vmem, arg, offset, size);
        if (ret) {
            break;
        }
        first_zero_bit = find_next_zero_bit(vmem->bitmap, vmem->bitmap_size,
                                            last_zero_bit + 2);
    }
    return ret;
}

/*
 * Adjust the memory section to cover the intersection with the given range.
 *
 * Returns false if the intersection is empty, otherwise returns true.
 */
static bool virito_mem_intersect_memory_section(MemoryRegionSection *s,
                                                uint64_t offset, uint64_t size)
{
    uint64_t start = MAX(s->offset_within_region, offset);
    uint64_t end = MIN(s->offset_within_region + int128_get64(s->size),
                       offset + size);

    if (end <= start) {
        return false;
    }

    s->offset_within_address_space += start - s->offset_within_region;
    s->offset_within_region = start;
    s->size = int128_make64(end - start);
    return true;
}

typedef int (*virtio_mem_section_cb)(MemoryRegionSection *s, void *arg);

static int virtio_mem_for_each_plugged_section(const VirtIOMEM *vmem,
                                               MemoryRegionSection *s,
                                               void *arg,
                                               virtio_mem_section_cb cb)
{
    unsigned long first_bit, last_bit;
    uint64_t offset, size;
    int ret = 0;

    first_bit = s->offset_within_region / vmem->block_size;
    first_bit = find_next_bit(vmem->bitmap, vmem->bitmap_size, first_bit);
    while (first_bit < vmem->bitmap_size) {
        MemoryRegionSection tmp = *s;

        offset = first_bit * vmem->block_size;
        last_bit = find_next_zero_bit(vmem->bitmap, vmem->bitmap_size,
                                      first_bit + 1) - 1;
        size = (last_bit - first_bit + 1) * vmem->block_size;

        if (!virito_mem_intersect_memory_section(&tmp, offset, size)) {
            break;
        }
        ret = cb(&tmp, arg);
        if (ret) {
            break;
        }
        first_bit = find_next_bit(vmem->bitmap, vmem->bitmap_size,
                                  last_bit + 2);
    }
    return ret;
}

static int virtio_mem_for_each_unplugged_section(const VirtIOMEM *vmem,
                                                 MemoryRegionSection *s,
                                                 void *arg,
                                                 virtio_mem_section_cb cb)
{
    unsigned long first_bit, last_bit;
    uint64_t offset, size;
    int ret = 0;

    first_bit = s->offset_within_region / vmem->block_size;
    first_bit = find_next_zero_bit(vmem->bitmap, vmem->bitmap_size, first_bit);
    while (first_bit < vmem->bitmap_size) {
        MemoryRegionSection tmp = *s;

        offset = first_bit * vmem->block_size;
        last_bit = find_next_bit(vmem->bitmap, vmem->bitmap_size,
                                 first_bit + 1) - 1;
        size = (last_bit - first_bit + 1) * vmem->block_size;

        if (!virito_mem_intersect_memory_section(&tmp, offset, size)) {
            break;
        }
        ret = cb(&tmp, arg);
        if (ret) {
            break;
        }
        first_bit = find_next_zero_bit(vmem->bitmap, vmem->bitmap_size,
                                       last_bit + 2);
    }
    return ret;
}

static int virtio_mem_notify_populate_cb(MemoryRegionSection *s, void *arg)
{
    RamDiscardListener *rdl = arg;

    return rdl->notify_populate(rdl, s);
}

static int virtio_mem_notify_discard_cb(MemoryRegionSection *s, void *arg)
{
    RamDiscardListener *rdl = arg;

    rdl->notify_discard(rdl, s);
    return 0;
}

static void virtio_mem_notify_unplug(VirtIOMEM *vmem, uint64_t offset,
                                     uint64_t size)
{
    RamDiscardListener *rdl;

    QLIST_FOREACH(rdl, &vmem->rdl_list, next) {
        MemoryRegionSection tmp = *rdl->section;

        if (!virito_mem_intersect_memory_section(&tmp, offset, size)) {
            continue;
        }
        rdl->notify_discard(rdl, &tmp);
    }
}

static int virtio_mem_notify_plug(VirtIOMEM *vmem, uint64_t offset,
                                  uint64_t size)
{
    RamDiscardListener *rdl, *rdl2;
    int ret = 0;

    QLIST_FOREACH(rdl, &vmem->rdl_list, next) {
        MemoryRegionSection tmp = *rdl->section;

        if (!virito_mem_intersect_memory_section(&tmp, offset, size)) {
            continue;
        }
        ret = rdl->notify_populate(rdl, &tmp);
        if (ret) {
            break;
        }
    }

    if (ret) {
        /* Notify all already-notified listeners. */
        QLIST_FOREACH(rdl2, &vmem->rdl_list, next) {
            MemoryRegionSection tmp = *rdl2->section;

            if (rdl2 == rdl) {
                break;
            }
            if (!virito_mem_intersect_memory_section(&tmp, offset, size)) {
                continue;
            }
            rdl2->notify_discard(rdl2, &tmp);
        }
    }
    return ret;
}

static void virtio_mem_notify_unplug_all(VirtIOMEM *vmem)
{
    RamDiscardListener *rdl;

    if (!vmem->size) {
        return;
    }

    QLIST_FOREACH(rdl, &vmem->rdl_list, next) {
        if (rdl->double_discard_supported) {
            rdl->notify_discard(rdl, rdl->section);
        } else {
            virtio_mem_for_each_plugged_section(vmem, rdl->section, rdl,
                                                virtio_mem_notify_discard_cb);
        }
    }
}

static bool virtio_mem_test_bitmap(const VirtIOMEM *vmem, uint64_t start_gpa,
                                   uint64_t size, bool plugged)
{
    const unsigned long first_bit = (start_gpa - vmem->addr) / vmem->block_size;
    const unsigned long last_bit = first_bit + (size / vmem->block_size) - 1;
    unsigned long found_bit;

    /* We fake a shorter bitmap to avoid searching too far. */
    if (plugged) {
        found_bit = find_next_zero_bit(vmem->bitmap, last_bit + 1, first_bit);
    } else {
        found_bit = find_next_bit(vmem->bitmap, last_bit + 1, first_bit);
    }
    return found_bit > last_bit;
}

static void virtio_mem_set_bitmap(VirtIOMEM *vmem, uint64_t start_gpa,
                                  uint64_t size, bool plugged)
{
    const unsigned long bit = (start_gpa - vmem->addr) / vmem->block_size;
    const unsigned long nbits = size / vmem->block_size;

    if (plugged) {
        bitmap_set(vmem->bitmap, bit, nbits);
    } else {
        bitmap_clear(vmem->bitmap, bit, nbits);
    }
}

static void virtio_mem_send_response(VirtIOMEM *vmem, VirtQueueElement *elem,
                                     struct virtio_mem_resp *resp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(vmem);
    VirtQueue *vq = vmem->vq;

    trace_virtio_mem_send_response(le16_to_cpu(resp->type));
    iov_from_buf(elem->in_sg, elem->in_num, 0, resp, sizeof(*resp));

    virtqueue_push(vq, elem, sizeof(*resp));
    virtio_notify(vdev, vq);
}

static void virtio_mem_send_response_simple(VirtIOMEM *vmem,
                                            VirtQueueElement *elem,
                                            uint16_t type)
{
    struct virtio_mem_resp resp = {
        .type = cpu_to_le16(type),
    };

    virtio_mem_send_response(vmem, elem, &resp);
}

static bool virtio_mem_valid_range(const VirtIOMEM *vmem, uint64_t gpa,
                                   uint64_t size)
{
    if (!QEMU_IS_ALIGNED(gpa, vmem->block_size)) {
        return false;
    }
    if (gpa + size < gpa || !size) {
        return false;
    }
    if (gpa < vmem->addr || gpa >= vmem->addr + vmem->usable_region_size) {
        return false;
    }
    if (gpa + size > vmem->addr + vmem->usable_region_size) {
        return false;
    }
    return true;
}

static int virtio_mem_set_block_state(VirtIOMEM *vmem, uint64_t start_gpa,
                                      uint64_t size, bool plug)
{
    const uint64_t offset = start_gpa - vmem->addr;
    RAMBlock *rb = vmem->memdev->mr.ram_block;

    if (virtio_mem_is_busy()) {
        return -EBUSY;
    }

    if (!plug) {
        if (ram_block_discard_range(rb, offset, size)) {
            return -EBUSY;
        }
        virtio_mem_notify_unplug(vmem, offset, size);
    } else {
        int ret = 0;

        if (vmem->prealloc) {
            void *area = memory_region_get_ram_ptr(&vmem->memdev->mr) + offset;
            int fd = memory_region_get_fd(&vmem->memdev->mr);
            Error *local_err = NULL;

            qemu_prealloc_mem(fd, area, size, 1, NULL, &local_err);
            if (local_err) {
                static bool warned;

                /*
                 * Warn only once, we don't want to fill the log with these
                 * warnings.
                 */
                if (!warned) {
                    warn_report_err(local_err);
                    warned = true;
                } else {
                    error_free(local_err);
                }
                ret = -EBUSY;
            }
        }
        if (!ret) {
            ret = virtio_mem_notify_plug(vmem, offset, size);
        }

        if (ret) {
            /* Could be preallocation or a notifier populated memory. */
            ram_block_discard_range(vmem->memdev->mr.ram_block, offset, size);
            return -EBUSY;
        }
    }
    virtio_mem_set_bitmap(vmem, start_gpa, size, plug);
    return 0;
}

static int virtio_mem_state_change_request(VirtIOMEM *vmem, uint64_t gpa,
                                           uint16_t nb_blocks, bool plug)
{
    const uint64_t size = nb_blocks * vmem->block_size;
    int ret;

    if (!virtio_mem_valid_range(vmem, gpa, size)) {
        return VIRTIO_MEM_RESP_ERROR;
    }

    if (plug && (vmem->size + size > vmem->requested_size)) {
        return VIRTIO_MEM_RESP_NACK;
    }

    /* test if really all blocks are in the opposite state */
    if (!virtio_mem_test_bitmap(vmem, gpa, size, !plug)) {
        return VIRTIO_MEM_RESP_ERROR;
    }

    ret = virtio_mem_set_block_state(vmem, gpa, size, plug);
    if (ret) {
        return VIRTIO_MEM_RESP_BUSY;
    }
    if (plug) {
        vmem->size += size;
    } else {
        vmem->size -= size;
    }
    notifier_list_notify(&vmem->size_change_notifiers, &vmem->size);
    return VIRTIO_MEM_RESP_ACK;
}

static void virtio_mem_plug_request(VirtIOMEM *vmem, VirtQueueElement *elem,
                                    struct virtio_mem_req *req)
{
    const uint64_t gpa = le64_to_cpu(req->u.plug.addr);
    const uint16_t nb_blocks = le16_to_cpu(req->u.plug.nb_blocks);
    uint16_t type;

    trace_virtio_mem_plug_request(gpa, nb_blocks);
    type = virtio_mem_state_change_request(vmem, gpa, nb_blocks, true);
    virtio_mem_send_response_simple(vmem, elem, type);
}

static void virtio_mem_unplug_request(VirtIOMEM *vmem, VirtQueueElement *elem,
                                      struct virtio_mem_req *req)
{
    const uint64_t gpa = le64_to_cpu(req->u.unplug.addr);
    const uint16_t nb_blocks = le16_to_cpu(req->u.unplug.nb_blocks);
    uint16_t type;

    trace_virtio_mem_unplug_request(gpa, nb_blocks);
    type = virtio_mem_state_change_request(vmem, gpa, nb_blocks, false);
    virtio_mem_send_response_simple(vmem, elem, type);
}

static void virtio_mem_resize_usable_region(VirtIOMEM *vmem,
                                            uint64_t requested_size,
                                            bool can_shrink)
{
    uint64_t newsize = MIN(memory_region_size(&vmem->memdev->mr),
                           requested_size + VIRTIO_MEM_USABLE_EXTENT);

    /* The usable region size always has to be multiples of the block size. */
    newsize = QEMU_ALIGN_UP(newsize, vmem->block_size);

    if (!requested_size) {
        newsize = 0;
    }

    if (newsize < vmem->usable_region_size && !can_shrink) {
        return;
    }

    trace_virtio_mem_resized_usable_region(vmem->usable_region_size, newsize);
    vmem->usable_region_size = newsize;
}

static int virtio_mem_unplug_all(VirtIOMEM *vmem)
{
    RAMBlock *rb = vmem->memdev->mr.ram_block;

    if (virtio_mem_is_busy()) {
        return -EBUSY;
    }

    if (ram_block_discard_range(rb, 0, qemu_ram_get_used_length(rb))) {
        return -EBUSY;
    }
    virtio_mem_notify_unplug_all(vmem);

    bitmap_clear(vmem->bitmap, 0, vmem->bitmap_size);
    if (vmem->size) {
        vmem->size = 0;
        notifier_list_notify(&vmem->size_change_notifiers, &vmem->size);
    }
    trace_virtio_mem_unplugged_all();
    virtio_mem_resize_usable_region(vmem, vmem->requested_size, true);
    return 0;
}

static void virtio_mem_unplug_all_request(VirtIOMEM *vmem,
                                          VirtQueueElement *elem)
{
    trace_virtio_mem_unplug_all_request();
    if (virtio_mem_unplug_all(vmem)) {
        virtio_mem_send_response_simple(vmem, elem, VIRTIO_MEM_RESP_BUSY);
    } else {
        virtio_mem_send_response_simple(vmem, elem, VIRTIO_MEM_RESP_ACK);
    }
}

static void virtio_mem_state_request(VirtIOMEM *vmem, VirtQueueElement *elem,
                                     struct virtio_mem_req *req)
{
    const uint16_t nb_blocks = le16_to_cpu(req->u.state.nb_blocks);
    const uint64_t gpa = le64_to_cpu(req->u.state.addr);
    const uint64_t size = nb_blocks * vmem->block_size;
    struct virtio_mem_resp resp = {
        .type = cpu_to_le16(VIRTIO_MEM_RESP_ACK),
    };

    trace_virtio_mem_state_request(gpa, nb_blocks);
    if (!virtio_mem_valid_range(vmem, gpa, size)) {
        virtio_mem_send_response_simple(vmem, elem, VIRTIO_MEM_RESP_ERROR);
        return;
    }

    if (virtio_mem_test_bitmap(vmem, gpa, size, true)) {
        resp.u.state.state = cpu_to_le16(VIRTIO_MEM_STATE_PLUGGED);
    } else if (virtio_mem_test_bitmap(vmem, gpa, size, false)) {
        resp.u.state.state = cpu_to_le16(VIRTIO_MEM_STATE_UNPLUGGED);
    } else {
        resp.u.state.state = cpu_to_le16(VIRTIO_MEM_STATE_MIXED);
    }
    trace_virtio_mem_state_response(le16_to_cpu(resp.u.state.state));
    virtio_mem_send_response(vmem, elem, &resp);
}

static void virtio_mem_handle_request(VirtIODevice *vdev, VirtQueue *vq)
{
    const int len = sizeof(struct virtio_mem_req);
    VirtIOMEM *vmem = VIRTIO_MEM(vdev);
    VirtQueueElement *elem;
    struct virtio_mem_req req;
    uint16_t type;

    while (true) {
        elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
        if (!elem) {
            return;
        }

        if (iov_to_buf(elem->out_sg, elem->out_num, 0, &req, len) < len) {
            virtio_error(vdev, "virtio-mem protocol violation: invalid request"
                         " size: %d", len);
            virtqueue_detach_element(vq, elem, 0);
            g_free(elem);
            return;
        }

        if (iov_size(elem->in_sg, elem->in_num) <
            sizeof(struct virtio_mem_resp)) {
            virtio_error(vdev, "virtio-mem protocol violation: not enough space"
                         " for response: %zu",
                         iov_size(elem->in_sg, elem->in_num));
            virtqueue_detach_element(vq, elem, 0);
            g_free(elem);
            return;
        }

        type = le16_to_cpu(req.type);
        switch (type) {
        case VIRTIO_MEM_REQ_PLUG:
            virtio_mem_plug_request(vmem, elem, &req);
            break;
        case VIRTIO_MEM_REQ_UNPLUG:
            virtio_mem_unplug_request(vmem, elem, &req);
            break;
        case VIRTIO_MEM_REQ_UNPLUG_ALL:
            virtio_mem_unplug_all_request(vmem, elem);
            break;
        case VIRTIO_MEM_REQ_STATE:
            virtio_mem_state_request(vmem, elem, &req);
            break;
        default:
            virtio_error(vdev, "virtio-mem protocol violation: unknown request"
                         " type: %d", type);
            virtqueue_detach_element(vq, elem, 0);
            g_free(elem);
            return;
        }

        g_free(elem);
    }
}

static void virtio_mem_get_config(VirtIODevice *vdev, uint8_t *config_data)
{
    VirtIOMEM *vmem = VIRTIO_MEM(vdev);
    struct virtio_mem_config *config = (void *) config_data;

    config->block_size = cpu_to_le64(vmem->block_size);
    config->node_id = cpu_to_le16(vmem->node);
    config->requested_size = cpu_to_le64(vmem->requested_size);
    config->plugged_size = cpu_to_le64(vmem->size);
    config->addr = cpu_to_le64(vmem->addr);
    config->region_size = cpu_to_le64(memory_region_size(&vmem->memdev->mr));
    config->usable_region_size = cpu_to_le64(vmem->usable_region_size);
}

static uint64_t virtio_mem_get_features(VirtIODevice *vdev, uint64_t features,
                                        Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    VirtIOMEM *vmem = VIRTIO_MEM(vdev);

    if (ms->numa_state) {
#if defined(CONFIG_ACPI)
        virtio_add_feature(&features, VIRTIO_MEM_F_ACPI_PXM);
#endif
    }
    assert(vmem->unplugged_inaccessible != ON_OFF_AUTO_AUTO);
    if (vmem->unplugged_inaccessible == ON_OFF_AUTO_ON) {
        virtio_add_feature(&features, VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE);
    }
    return features;
}

static int virtio_mem_validate_features(VirtIODevice *vdev)
{
    if (virtio_host_has_feature(vdev, VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE) &&
        !virtio_vdev_has_feature(vdev, VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE)) {
        return -EFAULT;
    }
    return 0;
}

static void virtio_mem_system_reset(void *opaque)
{
    VirtIOMEM *vmem = VIRTIO_MEM(opaque);

    /*
     * During usual resets, we will unplug all memory and shrink the usable
     * region size. This is, however, not possible in all scenarios. Then,
     * the guest has to deal with this manually (VIRTIO_MEM_REQ_UNPLUG_ALL).
     */
    virtio_mem_unplug_all(vmem);
}

static void virtio_mem_device_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    int nb_numa_nodes = ms->numa_state ? ms->numa_state->num_nodes : 0;
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOMEM *vmem = VIRTIO_MEM(dev);
    uint64_t page_size;
    RAMBlock *rb;
    int ret;

    if (!vmem->memdev) {
        error_setg(errp, "'%s' property is not set", VIRTIO_MEM_MEMDEV_PROP);
        return;
    } else if (host_memory_backend_is_mapped(vmem->memdev)) {
        error_setg(errp, "'%s' property specifies a busy memdev: %s",
                   VIRTIO_MEM_MEMDEV_PROP,
                   object_get_canonical_path_component(OBJECT(vmem->memdev)));
        return;
    } else if (!memory_region_is_ram(&vmem->memdev->mr) ||
        memory_region_is_rom(&vmem->memdev->mr) ||
        !vmem->memdev->mr.ram_block) {
        error_setg(errp, "'%s' property specifies an unsupported memdev",
                   VIRTIO_MEM_MEMDEV_PROP);
        return;
    }

    if ((nb_numa_nodes && vmem->node >= nb_numa_nodes) ||
        (!nb_numa_nodes && vmem->node)) {
        error_setg(errp, "'%s' property has value '%" PRIu32 "', which exceeds"
                   "the number of numa nodes: %d", VIRTIO_MEM_NODE_PROP,
                   vmem->node, nb_numa_nodes ? nb_numa_nodes : 1);
        return;
    }

    if (enable_mlock) {
        error_setg(errp, "Incompatible with mlock");
        return;
    }

    rb = vmem->memdev->mr.ram_block;
    page_size = qemu_ram_pagesize(rb);

#if defined(VIRTIO_MEM_HAS_LEGACY_GUESTS)
    switch (vmem->unplugged_inaccessible) {
    case ON_OFF_AUTO_AUTO:
        if (virtio_mem_has_shared_zeropage(rb)) {
            vmem->unplugged_inaccessible = ON_OFF_AUTO_OFF;
        } else {
            vmem->unplugged_inaccessible = ON_OFF_AUTO_ON;
        }
        break;
    case ON_OFF_AUTO_OFF:
        if (!virtio_mem_has_shared_zeropage(rb)) {
            warn_report("'%s' property set to 'off' with a memdev that does"
                        " not support the shared zeropage.",
                        VIRTIO_MEM_UNPLUGGED_INACCESSIBLE_PROP);
        }
        break;
    default:
        break;
    }
#else /* VIRTIO_MEM_HAS_LEGACY_GUESTS */
    vmem->unplugged_inaccessible = ON_OFF_AUTO_ON;
#endif /* VIRTIO_MEM_HAS_LEGACY_GUESTS */

    /*
     * If the block size wasn't configured by the user, use a sane default. This
     * allows using hugetlbfs backends of any page size without manual
     * intervention.
     */
    if (!vmem->block_size) {
        vmem->block_size = virtio_mem_default_block_size(rb);
    }

    if (vmem->block_size < page_size) {
        error_setg(errp, "'%s' property has to be at least the page size (0x%"
                   PRIx64 ")", VIRTIO_MEM_BLOCK_SIZE_PROP, page_size);
        return;
    } else if (vmem->block_size < virtio_mem_default_block_size(rb)) {
        warn_report("'%s' property is smaller than the default block size (%"
                    PRIx64 " MiB)", VIRTIO_MEM_BLOCK_SIZE_PROP,
                    virtio_mem_default_block_size(rb) / MiB);
    }
    if (!QEMU_IS_ALIGNED(vmem->requested_size, vmem->block_size)) {
        error_setg(errp, "'%s' property has to be multiples of '%s' (0x%" PRIx64
                   ")", VIRTIO_MEM_REQUESTED_SIZE_PROP,
                   VIRTIO_MEM_BLOCK_SIZE_PROP, vmem->block_size);
        return;
    } else if (!QEMU_IS_ALIGNED(vmem->addr, vmem->block_size)) {
        error_setg(errp, "'%s' property has to be multiples of '%s' (0x%" PRIx64
                   ")", VIRTIO_MEM_ADDR_PROP, VIRTIO_MEM_BLOCK_SIZE_PROP,
                   vmem->block_size);
        return;
    } else if (!QEMU_IS_ALIGNED(memory_region_size(&vmem->memdev->mr),
                                vmem->block_size)) {
        error_setg(errp, "'%s' property memdev size has to be multiples of"
                   "'%s' (0x%" PRIx64 ")", VIRTIO_MEM_MEMDEV_PROP,
                   VIRTIO_MEM_BLOCK_SIZE_PROP, vmem->block_size);
        return;
    }

    if (ram_block_coordinated_discard_require(true)) {
        error_setg(errp, "Discarding RAM is disabled");
        return;
    }

    ret = ram_block_discard_range(rb, 0, qemu_ram_get_used_length(rb));
    if (ret) {
        error_setg_errno(errp, -ret, "Unexpected error discarding RAM");
        ram_block_coordinated_discard_require(false);
        return;
    }

    virtio_mem_resize_usable_region(vmem, vmem->requested_size, true);

    vmem->bitmap_size = memory_region_size(&vmem->memdev->mr) /
                        vmem->block_size;
    vmem->bitmap = bitmap_new(vmem->bitmap_size);

    virtio_init(vdev, VIRTIO_ID_MEM, sizeof(struct virtio_mem_config));
    vmem->vq = virtio_add_queue(vdev, 128, virtio_mem_handle_request);

    host_memory_backend_set_mapped(vmem->memdev, true);
    vmstate_register_ram(&vmem->memdev->mr, DEVICE(vmem));
    qemu_register_reset(virtio_mem_system_reset, vmem);

    /*
     * Set ourselves as RamDiscardManager before the plug handler maps the
     * memory region and exposes it via an address space.
     */
    memory_region_set_ram_discard_manager(&vmem->memdev->mr,
                                          RAM_DISCARD_MANAGER(vmem));
}

static void virtio_mem_device_unrealize(DeviceState *dev)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOMEM *vmem = VIRTIO_MEM(dev);

    /*
     * The unplug handler unmapped the memory region, it cannot be
     * found via an address space anymore. Unset ourselves.
     */
    memory_region_set_ram_discard_manager(&vmem->memdev->mr, NULL);
    qemu_unregister_reset(virtio_mem_system_reset, vmem);
    vmstate_unregister_ram(&vmem->memdev->mr, DEVICE(vmem));
    host_memory_backend_set_mapped(vmem->memdev, false);
    virtio_del_queue(vdev, 0);
    virtio_cleanup(vdev);
    g_free(vmem->bitmap);
    ram_block_coordinated_discard_require(false);
}

static int virtio_mem_discard_range_cb(const VirtIOMEM *vmem, void *arg,
                                       uint64_t offset, uint64_t size)
{
    RAMBlock *rb = vmem->memdev->mr.ram_block;

    return ram_block_discard_range(rb, offset, size) ? -EINVAL : 0;
}

static int virtio_mem_restore_unplugged(VirtIOMEM *vmem)
{
    /* Make sure all memory is really discarded after migration. */
    return virtio_mem_for_each_unplugged_range(vmem, NULL,
                                               virtio_mem_discard_range_cb);
}

static int virtio_mem_post_load(void *opaque, int version_id)
{
    VirtIOMEM *vmem = VIRTIO_MEM(opaque);
    RamDiscardListener *rdl;
    int ret;

    /*
     * We started out with all memory discarded and our memory region is mapped
     * into an address space. Replay, now that we updated the bitmap.
     */
    QLIST_FOREACH(rdl, &vmem->rdl_list, next) {
        ret = virtio_mem_for_each_plugged_section(vmem, rdl->section, rdl,
                                                 virtio_mem_notify_populate_cb);
        if (ret) {
            return ret;
        }
    }

    if (migration_in_incoming_postcopy()) {
        return 0;
    }

    return virtio_mem_restore_unplugged(vmem);
}

typedef struct VirtIOMEMMigSanityChecks {
    VirtIOMEM *parent;
    uint64_t addr;
    uint64_t region_size;
    uint64_t block_size;
    uint32_t node;
} VirtIOMEMMigSanityChecks;

static int virtio_mem_mig_sanity_checks_pre_save(void *opaque)
{
    VirtIOMEMMigSanityChecks *tmp = opaque;
    VirtIOMEM *vmem = tmp->parent;

    tmp->addr = vmem->addr;
    tmp->region_size = memory_region_size(&vmem->memdev->mr);
    tmp->block_size = vmem->block_size;
    tmp->node = vmem->node;
    return 0;
}

static int virtio_mem_mig_sanity_checks_post_load(void *opaque, int version_id)
{
    VirtIOMEMMigSanityChecks *tmp = opaque;
    VirtIOMEM *vmem = tmp->parent;
    const uint64_t new_region_size = memory_region_size(&vmem->memdev->mr);

    if (tmp->addr != vmem->addr) {
        error_report("Property '%s' changed from 0x%" PRIx64 " to 0x%" PRIx64,
                     VIRTIO_MEM_ADDR_PROP, tmp->addr, vmem->addr);
        return -EINVAL;
    }
    /*
     * Note: Preparation for resizeable memory regions. The maximum size
     * of the memory region must not change during migration.
     */
    if (tmp->region_size != new_region_size) {
        error_report("Property '%s' size changed from 0x%" PRIx64 " to 0x%"
                     PRIx64, VIRTIO_MEM_MEMDEV_PROP, tmp->region_size,
                     new_region_size);
        return -EINVAL;
    }
    if (tmp->block_size != vmem->block_size) {
        error_report("Property '%s' changed from 0x%" PRIx64 " to 0x%" PRIx64,
                     VIRTIO_MEM_BLOCK_SIZE_PROP, tmp->block_size,
                     vmem->block_size);
        return -EINVAL;
    }
    if (tmp->node != vmem->node) {
        error_report("Property '%s' changed from %" PRIu32 " to %" PRIu32,
                     VIRTIO_MEM_NODE_PROP, tmp->node, vmem->node);
        return -EINVAL;
    }
    return 0;
}

static const VMStateDescription vmstate_virtio_mem_sanity_checks = {
    .name = "virtio-mem-device/sanity-checks",
    .pre_save = virtio_mem_mig_sanity_checks_pre_save,
    .post_load = virtio_mem_mig_sanity_checks_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(addr, VirtIOMEMMigSanityChecks),
        VMSTATE_UINT64(region_size, VirtIOMEMMigSanityChecks),
        VMSTATE_UINT64(block_size, VirtIOMEMMigSanityChecks),
        VMSTATE_UINT32(node, VirtIOMEMMigSanityChecks),
        VMSTATE_END_OF_LIST(),
    },
};

static const VMStateDescription vmstate_virtio_mem_device = {
    .name = "virtio-mem-device",
    .minimum_version_id = 1,
    .version_id = 1,
    .priority = MIG_PRI_VIRTIO_MEM,
    .post_load = virtio_mem_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_WITH_TMP(VirtIOMEM, VirtIOMEMMigSanityChecks,
                         vmstate_virtio_mem_sanity_checks),
        VMSTATE_UINT64(usable_region_size, VirtIOMEM),
        VMSTATE_UINT64(size, VirtIOMEM),
        VMSTATE_UINT64(requested_size, VirtIOMEM),
        VMSTATE_BITMAP(bitmap, VirtIOMEM, 0, bitmap_size),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription vmstate_virtio_mem = {
    .name = "virtio-mem",
    .minimum_version_id = 1,
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_VIRTIO_DEVICE,
        VMSTATE_END_OF_LIST()
    },
};

static void virtio_mem_fill_device_info(const VirtIOMEM *vmem,
                                        VirtioMEMDeviceInfo *vi)
{
    vi->memaddr = vmem->addr;
    vi->node = vmem->node;
    vi->requested_size = vmem->requested_size;
    vi->size = vmem->size;
    vi->max_size = memory_region_size(&vmem->memdev->mr);
    vi->block_size = vmem->block_size;
    vi->memdev = object_get_canonical_path(OBJECT(vmem->memdev));
}

static MemoryRegion *virtio_mem_get_memory_region(VirtIOMEM *vmem, Error **errp)
{
    if (!vmem->memdev) {
        error_setg(errp, "'%s' property must be set", VIRTIO_MEM_MEMDEV_PROP);
        return NULL;
    }

    return &vmem->memdev->mr;
}

static void virtio_mem_add_size_change_notifier(VirtIOMEM *vmem,
                                                Notifier *notifier)
{
    notifier_list_add(&vmem->size_change_notifiers, notifier);
}

static void virtio_mem_remove_size_change_notifier(VirtIOMEM *vmem,
                                                   Notifier *notifier)
{
    notifier_remove(notifier);
}

static void virtio_mem_get_size(Object *obj, Visitor *v, const char *name,
                                void *opaque, Error **errp)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(obj);
    uint64_t value = vmem->size;

    visit_type_size(v, name, &value, errp);
}

static void virtio_mem_get_requested_size(Object *obj, Visitor *v,
                                          const char *name, void *opaque,
                                          Error **errp)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(obj);
    uint64_t value = vmem->requested_size;

    visit_type_size(v, name, &value, errp);
}

static void virtio_mem_set_requested_size(Object *obj, Visitor *v,
                                          const char *name, void *opaque,
                                          Error **errp)
{
    VirtIOMEM *vmem = VIRTIO_MEM(obj);
    Error *err = NULL;
    uint64_t value;

    visit_type_size(v, name, &value, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    /*
     * The block size and memory backend are not fixed until the device was
     * realized. realize() will verify these properties then.
     */
    if (DEVICE(obj)->realized) {
        if (!QEMU_IS_ALIGNED(value, vmem->block_size)) {
            error_setg(errp, "'%s' has to be multiples of '%s' (0x%" PRIx64
                       ")", name, VIRTIO_MEM_BLOCK_SIZE_PROP,
                       vmem->block_size);
            return;
        } else if (value > memory_region_size(&vmem->memdev->mr)) {
            error_setg(errp, "'%s' cannot exceed the memory backend size"
                       "(0x%" PRIx64 ")", name,
                       memory_region_size(&vmem->memdev->mr));
            return;
        }

        if (value != vmem->requested_size) {
            virtio_mem_resize_usable_region(vmem, value, false);
            vmem->requested_size = value;
        }
        /*
         * Trigger a config update so the guest gets notified. We trigger
         * even if the size didn't change (especially helpful for debugging).
         */
        virtio_notify_config(VIRTIO_DEVICE(vmem));
    } else {
        vmem->requested_size = value;
    }
}

static void virtio_mem_get_block_size(Object *obj, Visitor *v, const char *name,
                                      void *opaque, Error **errp)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(obj);
    uint64_t value = vmem->block_size;

    /*
     * If not configured by the user (and we're not realized yet), use the
     * default block size we would use with the current memory backend.
     */
    if (!value) {
        if (vmem->memdev && memory_region_is_ram(&vmem->memdev->mr)) {
            value = virtio_mem_default_block_size(vmem->memdev->mr.ram_block);
        } else {
            value = virtio_mem_thp_size();
        }
    }

    visit_type_size(v, name, &value, errp);
}

static void virtio_mem_set_block_size(Object *obj, Visitor *v, const char *name,
                                      void *opaque, Error **errp)
{
    VirtIOMEM *vmem = VIRTIO_MEM(obj);
    Error *err = NULL;
    uint64_t value;

    if (DEVICE(obj)->realized) {
        error_setg(errp, "'%s' cannot be changed", name);
        return;
    }

    visit_type_size(v, name, &value, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    if (value < VIRTIO_MEM_MIN_BLOCK_SIZE) {
        error_setg(errp, "'%s' property has to be at least 0x%" PRIx32, name,
                   VIRTIO_MEM_MIN_BLOCK_SIZE);
        return;
    } else if (!is_power_of_2(value)) {
        error_setg(errp, "'%s' property has to be a power of two", name);
        return;
    }
    vmem->block_size = value;
}

static void virtio_mem_instance_init(Object *obj)
{
    VirtIOMEM *vmem = VIRTIO_MEM(obj);

    notifier_list_init(&vmem->size_change_notifiers);
    QLIST_INIT(&vmem->rdl_list);

    object_property_add(obj, VIRTIO_MEM_SIZE_PROP, "size", virtio_mem_get_size,
                        NULL, NULL, NULL);
    object_property_add(obj, VIRTIO_MEM_REQUESTED_SIZE_PROP, "size",
                        virtio_mem_get_requested_size,
                        virtio_mem_set_requested_size, NULL, NULL);
    object_property_add(obj, VIRTIO_MEM_BLOCK_SIZE_PROP, "size",
                        virtio_mem_get_block_size, virtio_mem_set_block_size,
                        NULL, NULL);
}

static Property virtio_mem_properties[] = {
    DEFINE_PROP_UINT64(VIRTIO_MEM_ADDR_PROP, VirtIOMEM, addr, 0),
    DEFINE_PROP_UINT32(VIRTIO_MEM_NODE_PROP, VirtIOMEM, node, 0),
    DEFINE_PROP_BOOL(VIRTIO_MEM_PREALLOC_PROP, VirtIOMEM, prealloc, false),
    DEFINE_PROP_LINK(VIRTIO_MEM_MEMDEV_PROP, VirtIOMEM, memdev,
                     TYPE_MEMORY_BACKEND, HostMemoryBackend *),
#if defined(VIRTIO_MEM_HAS_LEGACY_GUESTS)
    DEFINE_PROP_ON_OFF_AUTO(VIRTIO_MEM_UNPLUGGED_INACCESSIBLE_PROP, VirtIOMEM,
                            unplugged_inaccessible, ON_OFF_AUTO_AUTO),
#endif
    DEFINE_PROP_END_OF_LIST(),
};

static uint64_t virtio_mem_rdm_get_min_granularity(const RamDiscardManager *rdm,
                                                   const MemoryRegion *mr)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(rdm);

    g_assert(mr == &vmem->memdev->mr);
    return vmem->block_size;
}

static bool virtio_mem_rdm_is_populated(const RamDiscardManager *rdm,
                                        const MemoryRegionSection *s)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(rdm);
    uint64_t start_gpa = vmem->addr + s->offset_within_region;
    uint64_t end_gpa = start_gpa + int128_get64(s->size);

    g_assert(s->mr == &vmem->memdev->mr);

    start_gpa = QEMU_ALIGN_DOWN(start_gpa, vmem->block_size);
    end_gpa = QEMU_ALIGN_UP(end_gpa, vmem->block_size);

    if (!virtio_mem_valid_range(vmem, start_gpa, end_gpa - start_gpa)) {
        return false;
    }

    return virtio_mem_test_bitmap(vmem, start_gpa, end_gpa - start_gpa, true);
}

struct VirtIOMEMReplayData {
    void *fn;
    void *opaque;
};

static int virtio_mem_rdm_replay_populated_cb(MemoryRegionSection *s, void *arg)
{
    struct VirtIOMEMReplayData *data = arg;

    return ((ReplayRamPopulate)data->fn)(s, data->opaque);
}

static int virtio_mem_rdm_replay_populated(const RamDiscardManager *rdm,
                                           MemoryRegionSection *s,
                                           ReplayRamPopulate replay_fn,
                                           void *opaque)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(rdm);
    struct VirtIOMEMReplayData data = {
        .fn = replay_fn,
        .opaque = opaque,
    };

    g_assert(s->mr == &vmem->memdev->mr);
    return virtio_mem_for_each_plugged_section(vmem, s, &data,
                                            virtio_mem_rdm_replay_populated_cb);
}

static int virtio_mem_rdm_replay_discarded_cb(MemoryRegionSection *s,
                                              void *arg)
{
    struct VirtIOMEMReplayData *data = arg;

    ((ReplayRamDiscard)data->fn)(s, data->opaque);
    return 0;
}

static void virtio_mem_rdm_replay_discarded(const RamDiscardManager *rdm,
                                            MemoryRegionSection *s,
                                            ReplayRamDiscard replay_fn,
                                            void *opaque)
{
    const VirtIOMEM *vmem = VIRTIO_MEM(rdm);
    struct VirtIOMEMReplayData data = {
        .fn = replay_fn,
        .opaque = opaque,
    };

    g_assert(s->mr == &vmem->memdev->mr);
    virtio_mem_for_each_unplugged_section(vmem, s, &data,
                                          virtio_mem_rdm_replay_discarded_cb);
}

static void virtio_mem_rdm_register_listener(RamDiscardManager *rdm,
                                             RamDiscardListener *rdl,
                                             MemoryRegionSection *s)
{
    VirtIOMEM *vmem = VIRTIO_MEM(rdm);
    int ret;

    g_assert(s->mr == &vmem->memdev->mr);
    rdl->section = memory_region_section_new_copy(s);

    QLIST_INSERT_HEAD(&vmem->rdl_list, rdl, next);
    ret = virtio_mem_for_each_plugged_section(vmem, rdl->section, rdl,
                                              virtio_mem_notify_populate_cb);
    if (ret) {
        error_report("%s: Replaying plugged ranges failed: %s", __func__,
                     strerror(-ret));
    }
}

static void virtio_mem_rdm_unregister_listener(RamDiscardManager *rdm,
                                               RamDiscardListener *rdl)
{
    VirtIOMEM *vmem = VIRTIO_MEM(rdm);

    g_assert(rdl->section->mr == &vmem->memdev->mr);
    if (vmem->size) {
        if (rdl->double_discard_supported) {
            rdl->notify_discard(rdl, rdl->section);
        } else {
            virtio_mem_for_each_plugged_section(vmem, rdl->section, rdl,
                                                virtio_mem_notify_discard_cb);
        }
    }

    memory_region_section_free_copy(rdl->section);
    rdl->section = NULL;
    QLIST_REMOVE(rdl, next);
}

static void virtio_mem_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOMEMClass *vmc = VIRTIO_MEM_CLASS(klass);
    RamDiscardManagerClass *rdmc = RAM_DISCARD_MANAGER_CLASS(klass);

    device_class_set_props(dc, virtio_mem_properties);
    dc->vmsd = &vmstate_virtio_mem;

    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    vdc->realize = virtio_mem_device_realize;
    vdc->unrealize = virtio_mem_device_unrealize;
    vdc->get_config = virtio_mem_get_config;
    vdc->get_features = virtio_mem_get_features;
    vdc->validate_features = virtio_mem_validate_features;
    vdc->vmsd = &vmstate_virtio_mem_device;

    vmc->fill_device_info = virtio_mem_fill_device_info;
    vmc->get_memory_region = virtio_mem_get_memory_region;
    vmc->add_size_change_notifier = virtio_mem_add_size_change_notifier;
    vmc->remove_size_change_notifier = virtio_mem_remove_size_change_notifier;

    rdmc->get_min_granularity = virtio_mem_rdm_get_min_granularity;
    rdmc->is_populated = virtio_mem_rdm_is_populated;
    rdmc->replay_populated = virtio_mem_rdm_replay_populated;
    rdmc->replay_discarded = virtio_mem_rdm_replay_discarded;
    rdmc->register_listener = virtio_mem_rdm_register_listener;
    rdmc->unregister_listener = virtio_mem_rdm_unregister_listener;
}

static const TypeInfo virtio_mem_info = {
    .name = TYPE_VIRTIO_MEM,
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOMEM),
    .instance_init = virtio_mem_instance_init,
    .class_init = virtio_mem_class_init,
    .class_size = sizeof(VirtIOMEMClass),
    .interfaces = (InterfaceInfo[]) {
        { TYPE_RAM_DISCARD_MANAGER },
        { }
    },
};

static void virtio_register_types(void)
{
    type_register_static(&virtio_mem_info);
}

type_init(virtio_register_types)
