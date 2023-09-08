/*
 * Toshiba TC6393XB I/O Controller.
 * Found in Sharp Zaurus SL-6000 (tosa) or some
 * Toshiba e-Series PDAs.
 *
 * Most features are currently unsupported!!!
 *
 * This code is licensed under the GNU GPL v2.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/host-utils.h"
#include "hw/irq.h"
#include "hw/display/tc6393xb.h"
#include "exec/memory.h"
#include "hw/block/flash.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "sysemu/blockdev.h"

#define IRQ_TC6393_NAND		0
#define IRQ_TC6393_MMC		1
#define IRQ_TC6393_OHCI		2
#define IRQ_TC6393_SERIAL	3
#define IRQ_TC6393_FB		4

#define	TC6393XB_NR_IRQS	8

#define TC6393XB_GPIOS  16

#define SCR_REVID	0x08		/* b Revision ID	*/
#define SCR_ISR		0x50		/* b Interrupt Status	*/
#define SCR_IMR		0x52		/* b Interrupt Mask	*/
#define SCR_IRR		0x54		/* b Interrupt Routing	*/
#define SCR_GPER	0x60		/* w GP Enable		*/
#define SCR_GPI_SR(i)	(0x64 + (i))	/* b3 GPI Status	*/
#define SCR_GPI_IMR(i)	(0x68 + (i))	/* b3 GPI INT Mask	*/
#define SCR_GPI_EDER(i)	(0x6c + (i))	/* b3 GPI Edge Detect Enable */
#define SCR_GPI_LIR(i)	(0x70 + (i))	/* b3 GPI Level Invert	*/
#define SCR_GPO_DSR(i)	(0x78 + (i))	/* b3 GPO Data Set	*/
#define SCR_GPO_DOECR(i) (0x7c + (i))	/* b3 GPO Data OE Control */
#define SCR_GP_IARCR(i)	(0x80 + (i))	/* b3 GP Internal Active Register Control */
#define SCR_GP_IARLCR(i) (0x84 + (i))	/* b3 GP INTERNAL Active Register Level Control */
#define SCR_GPI_BCR(i)	(0x88 + (i))	/* b3 GPI Buffer Control */
#define SCR_GPA_IARCR	0x8c		/* w GPa Internal Active Register Control */
#define SCR_GPA_IARLCR	0x90		/* w GPa Internal Active Register Level Control */
#define SCR_GPA_BCR	0x94		/* w GPa Buffer Control */
#define SCR_CCR		0x98		/* w Clock Control	*/
#define SCR_PLL2CR	0x9a		/* w PLL2 Control	*/
#define SCR_PLL1CR	0x9c		/* l PLL1 Control	*/
#define SCR_DIARCR	0xa0		/* b Device Internal Active Register Control */
#define SCR_DBOCR	0xa1		/* b Device Buffer Off Control */
#define SCR_FER		0xe0		/* b Function Enable	*/
#define SCR_MCR		0xe4		/* w Mode Control	*/
#define SCR_CONFIG	0xfc		/* b Configuration Control */
#define SCR_DEBUG	0xff		/* b Debug		*/

#define NAND_CFG_COMMAND    0x04    /* w Command        */
#define NAND_CFG_BASE       0x10    /* l Control Base Address */
#define NAND_CFG_INTP       0x3d    /* b Interrupt Pin  */
#define NAND_CFG_INTE       0x48    /* b Int Enable     */
#define NAND_CFG_EC         0x4a    /* b Event Control  */
#define NAND_CFG_ICC        0x4c    /* b Internal Clock Control */
#define NAND_CFG_ECCC       0x5b    /* b ECC Control    */
#define NAND_CFG_NFTC       0x60    /* b NAND Flash Transaction Control */
#define NAND_CFG_NFM        0x61    /* b NAND Flash Monitor */
#define NAND_CFG_NFPSC      0x62    /* b NAND Flash Power Supply Control */
#define NAND_CFG_NFDC       0x63    /* b NAND Flash Detect Control */

#define NAND_DATA   0x00        /* l Data       */
#define NAND_MODE   0x04        /* b Mode       */
#define NAND_STATUS 0x05        /* b Status     */
#define NAND_ISR    0x06        /* b Interrupt Status */
#define NAND_IMR    0x07        /* b Interrupt Mask */

#define NAND_MODE_WP        0x80
#define NAND_MODE_CE        0x10
#define NAND_MODE_ALE       0x02
#define NAND_MODE_CLE       0x01
#define NAND_MODE_ECC_MASK  0x60
#define NAND_MODE_ECC_EN    0x20
#define NAND_MODE_ECC_READ  0x40
#define NAND_MODE_ECC_RST   0x60

struct TC6393xbState {
    MemoryRegion iomem;
    qemu_irq irq;
    qemu_irq *sub_irqs;
    struct {
        uint8_t ISR;
        uint8_t IMR;
        uint8_t IRR;
        uint16_t GPER;
        uint8_t GPI_SR[3];
        uint8_t GPI_IMR[3];
        uint8_t GPI_EDER[3];
        uint8_t GPI_LIR[3];
        uint8_t GP_IARCR[3];
        uint8_t GP_IARLCR[3];
        uint8_t GPI_BCR[3];
        uint16_t GPA_IARCR;
        uint16_t GPA_IARLCR;
        uint16_t CCR;
        uint16_t PLL2CR;
        uint32_t PLL1CR;
        uint8_t DIARCR;
        uint8_t DBOCR;
        uint8_t FER;
        uint16_t MCR;
        uint8_t CONFIG;
        uint8_t DEBUG;
    } scr;
    uint32_t gpio_dir;
    uint32_t gpio_level;
    uint32_t prev_level;
    qemu_irq handler[TC6393XB_GPIOS];
    qemu_irq *gpio_in;

    struct {
        uint8_t mode;
        uint8_t isr;
        uint8_t imr;
    } nand;
    int nand_enable;
    uint32_t nand_phys;
    DeviceState *flash;
    ECCState ecc;

    QemuConsole *con;
    MemoryRegion vram;
    uint16_t *vram_ptr;
    uint32_t scr_width, scr_height; /* in pixels */
    qemu_irq l3v;
    unsigned blank : 1,
             blanked : 1;
};

static void tc6393xb_gpio_set(void *opaque, int line, int level)
{
//    TC6393xbState *s = opaque;

    if (line > TC6393XB_GPIOS) {
        printf("%s: No GPIO pin %i\n", __func__, line);
        return;
    }

    // FIXME: how does the chip reflect the GPIO input level change?
}

static void tc6393xb_gpio_handler_update(TC6393xbState *s)
{
    uint32_t level, diff;
    int bit;

    level = s->gpio_level & s->gpio_dir;
    level &= MAKE_64BIT_MASK(0, TC6393XB_GPIOS);

    for (diff = s->prev_level ^ level; diff; diff ^= 1 << bit) {
        bit = ctz32(diff);
        qemu_set_irq(s->handler[bit], (level >> bit) & 1);
    }

    s->prev_level = level;
}

qemu_irq tc6393xb_l3v_get(TC6393xbState *s)
{
    return s->l3v;
}

static void tc6393xb_l3v(void *opaque, int line, int level)
{
    TC6393xbState *s = opaque;
    s->blank = !level;
    fprintf(stderr, "L3V: %d\n", level);
}

static void tc6393xb_sub_irq(void *opaque, int line, int level) {
    TC6393xbState *s = opaque;
    uint8_t isr = s->scr.ISR;
    if (level)
        isr |= 1 << line;
    else
        isr &= ~(1 << line);
    s->scr.ISR = isr;
    qemu_set_irq(s->irq, isr & s->scr.IMR);
}

#define SCR_REG_B(N)                            \
    case SCR_ ##N: return s->scr.N
#define SCR_REG_W(N)                            \
    case SCR_ ##N: return s->scr.N;             \
    case SCR_ ##N + 1: return s->scr.N >> 8;
#define SCR_REG_L(N)                            \
    case SCR_ ##N: return s->scr.N;             \
    case SCR_ ##N + 1: return s->scr.N >> 8;    \
    case SCR_ ##N + 2: return s->scr.N >> 16;   \
    case SCR_ ##N + 3: return s->scr.N >> 24;
#define SCR_REG_A(N)                            \
    case SCR_ ##N(0): return s->scr.N[0];       \
    case SCR_ ##N(1): return s->scr.N[1];       \
    case SCR_ ##N(2): return s->scr.N[2]

static uint32_t tc6393xb_scr_readb(TC6393xbState *s, hwaddr addr)
{
    switch (addr) {
        case SCR_REVID:
            return 3;
        case SCR_REVID+1:
            return 0;
        SCR_REG_B(ISR);
        SCR_REG_B(IMR);
        SCR_REG_B(IRR);
        SCR_REG_W(GPER);
        SCR_REG_A(GPI_SR);
        SCR_REG_A(GPI_IMR);
        SCR_REG_A(GPI_EDER);
        SCR_REG_A(GPI_LIR);
        case SCR_GPO_DSR(0):
        case SCR_GPO_DSR(1):
        case SCR_GPO_DSR(2):
            return (s->gpio_level >> ((addr - SCR_GPO_DSR(0)) * 8)) & 0xff;
        case SCR_GPO_DOECR(0):
        case SCR_GPO_DOECR(1):
        case SCR_GPO_DOECR(2):
            return (s->gpio_dir >> ((addr - SCR_GPO_DOECR(0)) * 8)) & 0xff;
        SCR_REG_A(GP_IARCR);
        SCR_REG_A(GP_IARLCR);
        SCR_REG_A(GPI_BCR);
        SCR_REG_W(GPA_IARCR);
        SCR_REG_W(GPA_IARLCR);
        SCR_REG_W(CCR);
        SCR_REG_W(PLL2CR);
        SCR_REG_L(PLL1CR);
        SCR_REG_B(DIARCR);
        SCR_REG_B(DBOCR);
        SCR_REG_B(FER);
        SCR_REG_W(MCR);
        SCR_REG_B(CONFIG);
        SCR_REG_B(DEBUG);
    }
    fprintf(stderr, "tc6393xb_scr: unhandled read at %08x\n", (uint32_t) addr);
    return 0;
}
#undef SCR_REG_B
#undef SCR_REG_W
#undef SCR_REG_L
#undef SCR_REG_A

#define SCR_REG_B(N)                                \
    case SCR_ ##N: s->scr.N = value; return;
#define SCR_REG_W(N)                                \
    case SCR_ ##N: s->scr.N = (s->scr.N & ~0xff) | (value & 0xff); return; \
    case SCR_ ##N + 1: s->scr.N = (s->scr.N & 0xff) | (value << 8); return
#define SCR_REG_L(N)                                \
    case SCR_ ##N: s->scr.N = (s->scr.N & ~0xff) | (value & 0xff); return;   \
    case SCR_ ##N + 1: s->scr.N = (s->scr.N & ~(0xff << 8)) | (value & (0xff << 8)); return;     \
    case SCR_ ##N + 2: s->scr.N = (s->scr.N & ~(0xff << 16)) | (value & (0xff << 16)); return;   \
    case SCR_ ##N + 3: s->scr.N = (s->scr.N & ~(0xff << 24)) | (value & (0xff << 24)); return;
#define SCR_REG_A(N)                                \
    case SCR_ ##N(0): s->scr.N[0] = value; return;   \
    case SCR_ ##N(1): s->scr.N[1] = value; return;   \
    case SCR_ ##N(2): s->scr.N[2] = value; return

static void tc6393xb_scr_writeb(TC6393xbState *s, hwaddr addr, uint32_t value)
{
    switch (addr) {
        SCR_REG_B(ISR);
        SCR_REG_B(IMR);
        SCR_REG_B(IRR);
        SCR_REG_W(GPER);
        SCR_REG_A(GPI_SR);
        SCR_REG_A(GPI_IMR);
        SCR_REG_A(GPI_EDER);
        SCR_REG_A(GPI_LIR);
        case SCR_GPO_DSR(0):
        case SCR_GPO_DSR(1):
        case SCR_GPO_DSR(2):
            s->gpio_level = (s->gpio_level & ~(0xff << ((addr - SCR_GPO_DSR(0))*8))) | ((value & 0xff) << ((addr - SCR_GPO_DSR(0))*8));
            tc6393xb_gpio_handler_update(s);
            return;
        case SCR_GPO_DOECR(0):
        case SCR_GPO_DOECR(1):
        case SCR_GPO_DOECR(2):
            s->gpio_dir = (s->gpio_dir & ~(0xff << ((addr - SCR_GPO_DOECR(0))*8))) | ((value & 0xff) << ((addr - SCR_GPO_DOECR(0))*8));
            tc6393xb_gpio_handler_update(s);
            return;
        SCR_REG_A(GP_IARCR);
        SCR_REG_A(GP_IARLCR);
        SCR_REG_A(GPI_BCR);
        SCR_REG_W(GPA_IARCR);
        SCR_REG_W(GPA_IARLCR);
        SCR_REG_W(CCR);
        SCR_REG_W(PLL2CR);
        SCR_REG_L(PLL1CR);
        SCR_REG_B(DIARCR);
        SCR_REG_B(DBOCR);
        SCR_REG_B(FER);
        SCR_REG_W(MCR);
        SCR_REG_B(CONFIG);
        SCR_REG_B(DEBUG);
    }
    fprintf(stderr, "tc6393xb_scr: unhandled write at %08x: %02x\n",
                                        (uint32_t) addr, value & 0xff);
}
#undef SCR_REG_B
#undef SCR_REG_W
#undef SCR_REG_L
#undef SCR_REG_A

static void tc6393xb_nand_irq(TC6393xbState *s) {
    qemu_set_irq(s->sub_irqs[IRQ_TC6393_NAND],
            (s->nand.imr & 0x80) && (s->nand.imr & s->nand.isr));
}

static uint32_t tc6393xb_nand_cfg_readb(TC6393xbState *s, hwaddr addr) {
    switch (addr) {
        case NAND_CFG_COMMAND:
            return s->nand_enable ? 2 : 0;
        case NAND_CFG_BASE:
        case NAND_CFG_BASE + 1:
        case NAND_CFG_BASE + 2:
        case NAND_CFG_BASE + 3:
            return s->nand_phys >> (addr - NAND_CFG_BASE);
    }
    fprintf(stderr, "tc6393xb_nand_cfg: unhandled read at %08x\n", (uint32_t) addr);
    return 0;
}
static void tc6393xb_nand_cfg_writeb(TC6393xbState *s, hwaddr addr, uint32_t value) {
    switch (addr) {
        case NAND_CFG_COMMAND:
            s->nand_enable = (value & 0x2);
            return;
        case NAND_CFG_BASE:
        case NAND_CFG_BASE + 1:
        case NAND_CFG_BASE + 2:
        case NAND_CFG_BASE + 3:
            s->nand_phys &= ~(0xff << ((addr - NAND_CFG_BASE) * 8));
            s->nand_phys |= (value & 0xff) << ((addr - NAND_CFG_BASE) * 8);
            return;
    }
    fprintf(stderr, "tc6393xb_nand_cfg: unhandled write at %08x: %02x\n",
                                        (uint32_t) addr, value & 0xff);
}

static uint32_t tc6393xb_nand_readb(TC6393xbState *s, hwaddr addr) {
    switch (addr) {
        case NAND_DATA + 0:
        case NAND_DATA + 1:
        case NAND_DATA + 2:
        case NAND_DATA + 3:
            return nand_getio(s->flash);
        case NAND_MODE:
            return s->nand.mode;
        case NAND_STATUS:
            return 0x14;
        case NAND_ISR:
            return s->nand.isr;
        case NAND_IMR:
            return s->nand.imr;
    }
    fprintf(stderr, "tc6393xb_nand: unhandled read at %08x\n", (uint32_t) addr);
    return 0;
}
static void tc6393xb_nand_writeb(TC6393xbState *s, hwaddr addr, uint32_t value) {
//    fprintf(stderr, "tc6393xb_nand: write at %08x: %02x\n",
//					(uint32_t) addr, value & 0xff);
    switch (addr) {
        case NAND_DATA + 0:
        case NAND_DATA + 1:
        case NAND_DATA + 2:
        case NAND_DATA + 3:
            nand_setio(s->flash, value);
            s->nand.isr |= 1;
            tc6393xb_nand_irq(s);
            return;
        case NAND_MODE:
            s->nand.mode = value;
            nand_setpins(s->flash,
                    value & NAND_MODE_CLE,
                    value & NAND_MODE_ALE,
                    !(value & NAND_MODE_CE),
                    value & NAND_MODE_WP,
                    0); // FIXME: gnd
            switch (value & NAND_MODE_ECC_MASK) {
                case NAND_MODE_ECC_RST:
                    ecc_reset(&s->ecc);
                    break;
                case NAND_MODE_ECC_READ:
                    // FIXME
                    break;
                case NAND_MODE_ECC_EN:
                    ecc_reset(&s->ecc);
            }
            return;
        case NAND_ISR:
            s->nand.isr = value;
            tc6393xb_nand_irq(s);
            return;
        case NAND_IMR:
            s->nand.imr = value;
            tc6393xb_nand_irq(s);
            return;
    }
    fprintf(stderr, "tc6393xb_nand: unhandled write at %08x: %02x\n",
                                        (uint32_t) addr, value & 0xff);
}

static void tc6393xb_draw_graphic(TC6393xbState *s, int full_update)
{
    DisplaySurface *surface = qemu_console_surface(s->con);
    int i;
    uint16_t *data_buffer;
    uint8_t *data_display;

    data_buffer = s->vram_ptr;
    data_display = surface_data(surface);
    for (i = 0; i < s->scr_height; i++) {
        int j;
        for (j = 0; j < s->scr_width; j++, data_display += 4, data_buffer++) {
            uint16_t color = *data_buffer;
            uint32_t dest_color = rgb_to_pixel32(
                           ((color & 0xf800) * 0x108) >> 11,
                           ((color & 0x7e0) * 0x41) >> 9,
                           ((color & 0x1f) * 0x21) >> 2
                           );
            *(uint32_t *)data_display = dest_color;
        }
    }
    dpy_gfx_update_full(s->con);
}

static void tc6393xb_draw_blank(TC6393xbState *s, int full_update)
{
    DisplaySurface *surface = qemu_console_surface(s->con);
    int i, w;
    uint8_t *d;

    if (!full_update)
        return;

    w = s->scr_width * surface_bytes_per_pixel(surface);
    d = surface_data(surface);
    for(i = 0; i < s->scr_height; i++) {
        memset(d, 0, w);
        d += surface_stride(surface);
    }

    dpy_gfx_update_full(s->con);
}

static void tc6393xb_update_display(void *opaque)
{
    TC6393xbState *s = opaque;
    DisplaySurface *surface = qemu_console_surface(s->con);
    int full_update;

    if (s->scr_width == 0 || s->scr_height == 0)
        return;

    full_update = 0;
    if (s->blanked != s->blank) {
        s->blanked = s->blank;
        full_update = 1;
    }
    if (s->scr_width != surface_width(surface) ||
        s->scr_height != surface_height(surface)) {
        qemu_console_resize(s->con, s->scr_width, s->scr_height);
        full_update = 1;
    }
    if (s->blanked)
        tc6393xb_draw_blank(s, full_update);
    else
        tc6393xb_draw_graphic(s, full_update);
}


static uint64_t tc6393xb_readb(void *opaque, hwaddr addr,
                               unsigned size)
{
    TC6393xbState *s = opaque;

    switch (addr >> 8) {
        case 0:
            return tc6393xb_scr_readb(s, addr & 0xff);
        case 1:
            return tc6393xb_nand_cfg_readb(s, addr & 0xff);
    };

    if ((addr &~0xff) == s->nand_phys && s->nand_enable) {
//        return tc6393xb_nand_readb(s, addr & 0xff);
        uint8_t d = tc6393xb_nand_readb(s, addr & 0xff);
//        fprintf(stderr, "tc6393xb_nand: read at %08x: %02hhx\n", (uint32_t) addr, d);
        return d;
    }

//    fprintf(stderr, "tc6393xb: unhandled read at %08x\n", (uint32_t) addr);
    return 0;
}

static void tc6393xb_writeb(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size) {
    TC6393xbState *s = opaque;

    switch (addr >> 8) {
        case 0:
            tc6393xb_scr_writeb(s, addr & 0xff, value);
            return;
        case 1:
            tc6393xb_nand_cfg_writeb(s, addr & 0xff, value);
            return;
    };

    if ((addr &~0xff) == s->nand_phys && s->nand_enable)
        tc6393xb_nand_writeb(s, addr & 0xff, value);
    else
        fprintf(stderr, "tc6393xb: unhandled write at %08x: %02x\n",
                (uint32_t) addr, (int)value & 0xff);
}

static const GraphicHwOps tc6393xb_gfx_ops = {
    .gfx_update  = tc6393xb_update_display,
};

TC6393xbState *tc6393xb_init(MemoryRegion *sysmem, uint32_t base, qemu_irq irq)
{
    TC6393xbState *s;
    DriveInfo *nand;
    static const MemoryRegionOps tc6393xb_ops = {
        .read = tc6393xb_readb,
        .write = tc6393xb_writeb,
        .endianness = DEVICE_NATIVE_ENDIAN,
        .impl = {
            .min_access_size = 1,
            .max_access_size = 1,
        },
    };

    s = g_new0(TC6393xbState, 1);
    s->irq = irq;
    s->gpio_in = qemu_allocate_irqs(tc6393xb_gpio_set, s, TC6393XB_GPIOS);

    s->l3v = qemu_allocate_irq(tc6393xb_l3v, s, 0);
    s->blanked = 1;

    s->sub_irqs = qemu_allocate_irqs(tc6393xb_sub_irq, s, TC6393XB_NR_IRQS);

    nand = drive_get(IF_MTD, 0, 0);
    s->flash = nand_init(nand ? blk_by_legacy_dinfo(nand) : NULL,
                         NAND_MFR_TOSHIBA, 0x76);

    memory_region_init_io(&s->iomem, NULL, &tc6393xb_ops, s, "tc6393xb", 0x10000);
    memory_region_add_subregion(sysmem, base, &s->iomem);

    memory_region_init_ram(&s->vram, NULL, "tc6393xb.vram", 0x100000,
                           &error_fatal);
    s->vram_ptr = memory_region_get_ram_ptr(&s->vram);
    memory_region_add_subregion(sysmem, base + 0x100000, &s->vram);
    s->scr_width = 480;
    s->scr_height = 640;
    s->con = graphic_console_init(NULL, 0, &tc6393xb_gfx_ops, s);

    return s;
}
