#ifndef HW_IDE_PCI_H
#define HW_IDE_PCI_H

#include "hw/ide/ide-bus.h"
#include "hw/pci/pci_device.h"
#include "qom/object.h"

#define BM_STATUS_DMAING 0x01
#define BM_STATUS_ERROR  0x02
#define BM_STATUS_INT    0x04

#define BM_CMD_START     0x01
#define BM_CMD_READ      0x08

typedef struct BMDMAState {
    IDEDMA dma;
    uint8_t cmd;
    uint8_t status;
    uint32_t addr;

    IDEBus *bus;
    /* current transfer state */
    uint32_t cur_addr;
    uint32_t cur_prd_last;
    uint32_t cur_prd_addr;
    uint32_t cur_prd_len;
    BlockCompletionFunc *dma_cb;
    MemoryRegion addr_ioport;
    MemoryRegion extra_io;
    qemu_irq irq;

    /* Bit 0-2 and 7:   BM status register
     * Bit 3-6:         bus->error_status */
    uint8_t migration_compat_status;
    uint8_t migration_retry_unit;
    int64_t migration_retry_sector_num;
    uint32_t migration_retry_nsector;

    struct PCIIDEState *pci_dev;
} BMDMAState;

#define TYPE_PCI_IDE "pci-ide"
OBJECT_DECLARE_SIMPLE_TYPE(PCIIDEState, PCI_IDE)

struct PCIIDEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    IDEBus bus[2];
    BMDMAState bmdma[2];
    qemu_irq isa_irq[2];
    uint32_t secondary; /* used only for cmd646 */
    MemoryRegion bmdma_bar;
    MemoryRegion cmd_bar[2];
    MemoryRegion data_bar[2];
};

void bmdma_init(IDEBus *bus, BMDMAState *bm, PCIIDEState *d);
void bmdma_cmd_writeb(BMDMAState *bm, uint32_t val);
void bmdma_status_writeb(BMDMAState *bm, uint32_t val);
extern MemoryRegionOps bmdma_addr_ioport_ops;
void pci_ide_create_devs(PCIDevice *dev);
void pci_ide_update_mode(PCIIDEState *s);

extern const VMStateDescription vmstate_ide_pci;
extern const MemoryRegionOps pci_ide_cmd_le_ops;
extern const MemoryRegionOps pci_ide_data_le_ops;
#endif
