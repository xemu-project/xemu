#ifndef HW_QXL_H
#define HW_QXL_H


#include "hw/pci/pci.h"
#include "vga_int.h"
#include "qemu/thread.h"

#include "ui/qemu-spice.h"
#include "ui/spice-display.h"
#include "qom/object.h"

enum qxl_mode {
    QXL_MODE_UNDEFINED,
    QXL_MODE_VGA,
    QXL_MODE_COMPAT, /* spice 0.4.x */
    QXL_MODE_NATIVE,
};

#ifndef QXL_VRAM64_RANGE_INDEX
#define QXL_VRAM64_RANGE_INDEX 4
#endif

#define QXL_UNDEFINED_IO UINT32_MAX

#define QXL_NUM_DIRTY_RECTS 64

#define QXL_PAGE_BITS 12
#define QXL_PAGE_SIZE (1 << QXL_PAGE_BITS);

struct PCIQXLDevice {
    PCIDevice          pci;
    PortioList         vga_port_list;
    SimpleSpiceDisplay ssd;
    int                id;
    bool               have_vga;
    uint32_t           debug;
    uint32_t           guestdebug;
    uint32_t           cmdlog;

    uint32_t           guest_bug;

    enum qxl_mode      mode;
    uint32_t           cmdflags;
    uint32_t           revision;

    int32_t            num_memslots;

    uint32_t           current_async;
    QemuMutex          async_lock;

    struct guest_slots {
        QXLMemSlot     slot;
        MemoryRegion   *mr;
        uint64_t       offset;
        uint64_t       size;
        uint64_t       delta;
        uint32_t       active;
    } guest_slots[NUM_MEMSLOTS];

    struct guest_primary {
        QXLSurfaceCreate surface;
        uint32_t       commands;
        uint32_t       resized;
        int32_t        qxl_stride;
        uint32_t       abs_stride;
        uint32_t       bits_pp;
        uint32_t       bytes_pp;
        uint8_t        *data;
    } guest_primary;

    struct surfaces {
        QXLPHYSICAL    *cmds;
        uint32_t       count;
        uint32_t       max;
    } guest_surfaces;
    QXLPHYSICAL        guest_cursor;

    QXLPHYSICAL        guest_monitors_config;
    uint32_t           guest_head0_width;
    uint32_t           guest_head0_height;

    QemuMutex          track_lock;

    /* thread signaling */
    QEMUBH             *update_irq;

    /* ram pci bar */
    QXLRam             *ram;
    VGACommonState     vga;
    uint32_t           num_free_res;
    QXLReleaseInfo     *last_release;
    uint32_t           last_release_offset;
    uint32_t           oom_running;
    uint32_t           vgamem_size;

    /* rom pci bar */
    QXLRom             shadow_rom;
    QXLRom             *rom;
    QXLModes           *modes;
    uint32_t           rom_size;
    MemoryRegion       rom_bar;
#if SPICE_SERVER_VERSION >= 0x000c06 /* release 0.12.6 */
    uint16_t           max_outputs;
#endif

    /* vram pci bar */
    uint64_t           vram_size;
    MemoryRegion       vram_bar;
    uint64_t           vram32_size;
    MemoryRegion       vram32_bar;

    /* io bar */
    MemoryRegion       io_bar;

    /* user-friendly properties (in megabytes) */
    uint32_t          ram_size_mb;
    uint32_t          vram_size_mb;
    uint32_t          vram32_size_mb;
    uint32_t          vgamem_size_mb;
    uint32_t          xres;
    uint32_t          yres;

    /* qxl_render_update state */
    int                render_update_cookie_num;
    int                num_dirty_rects;
    QXLRect            dirty[QXL_NUM_DIRTY_RECTS];
    QEMUBH            *update_area_bh;
};

#define TYPE_PCI_QXL "pci-qxl"
OBJECT_DECLARE_SIMPLE_TYPE(PCIQXLDevice, PCI_QXL)

#define PANIC_ON(x) if ((x)) {                         \
    printf("%s: PANIC %s failed\n", __func__, #x); \
    abort();                                           \
}

#define dprint(_qxl, _level, _fmt, ...)                                 \
    do {                                                                \
        if (_qxl->debug >= _level) {                                    \
            fprintf(stderr, "qxl-%d: ", _qxl->id);                      \
            fprintf(stderr, _fmt, ## __VA_ARGS__);                      \
        }                                                               \
    } while (0)

#define QXL_DEFAULT_REVISION (QXL_REVISION_STABLE_V12 + 1)

/* qxl.c */
/**
 * qxl_phys2virt: Get a pointer within a PCI VRAM memory region.
 *
 * @qxl: QXL device
 * @phys: physical offset of buffer within the VRAM
 * @group_id: memory slot group
 * @size: size of the buffer
 *
 * Returns a host pointer to a buffer placed at offset @phys within the
 * active slot @group_id of the PCI VGA RAM memory region associated with
 * the @qxl device. If the slot is inactive, or the offset + size are out
 * of the memory region, returns NULL.
 *
 * Use with care; by the time this function returns, the returned pointer is
 * not protected by RCU anymore.  If the caller is not within an RCU critical
 * section and does not hold the iothread lock, it must have other means of
 * protecting the pointer, such as a reference to the region that includes
 * the incoming ram_addr_t.
 *
 */
void *qxl_phys2virt(PCIQXLDevice *qxl, QXLPHYSICAL phys, int group_id,
                    size_t size);
void qxl_set_guest_bug(PCIQXLDevice *qxl, const char *msg, ...)
    G_GNUC_PRINTF(2, 3);

void qxl_spice_update_area(PCIQXLDevice *qxl, uint32_t surface_id,
                           struct QXLRect *area, struct QXLRect *dirty_rects,
                           uint32_t num_dirty_rects,
                           uint32_t clear_dirty_region,
                           qxl_async_io async, QXLCookie *cookie);
void qxl_spice_loadvm_commands(PCIQXLDevice *qxl, struct QXLCommandExt *ext,
                               uint32_t count);
void qxl_spice_oom(PCIQXLDevice *qxl);
void qxl_spice_reset_memslots(PCIQXLDevice *qxl);
void qxl_spice_reset_image_cache(PCIQXLDevice *qxl);
void qxl_spice_reset_cursor(PCIQXLDevice *qxl);

/* qxl-logger.c */
int qxl_log_cmd_cursor(PCIQXLDevice *qxl, QXLCursorCmd *cmd, int group_id);
int qxl_log_command(PCIQXLDevice *qxl, const char *ring, QXLCommandExt *ext);

/* qxl-render.c */
void qxl_render_resize(PCIQXLDevice *qxl);
void qxl_render_update(PCIQXLDevice *qxl);
int qxl_render_cursor(PCIQXLDevice *qxl, QXLCommandExt *ext);
void qxl_render_update_area_done(PCIQXLDevice *qxl, QXLCookie *cookie);
void qxl_render_update_area_bh(void *opaque);

#endif
