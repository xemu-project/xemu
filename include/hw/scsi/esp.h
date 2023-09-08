#ifndef QEMU_HW_ESP_H
#define QEMU_HW_ESP_H

#include "hw/scsi/scsi.h"
#include "hw/sysbus.h"
#include "qemu/fifo8.h"
#include "qom/object.h"

/* esp.c */
#define ESP_MAX_DEVS 7
typedef void (*ESPDMAMemoryReadWriteFunc)(void *opaque, uint8_t *buf, int len);

#define ESP_REGS 16
#define ESP_FIFO_SZ 16
#define ESP_CMDFIFO_SZ 32

typedef struct ESPState ESPState;

#define TYPE_ESP "esp"
OBJECT_DECLARE_SIMPLE_TYPE(ESPState, ESP)

struct ESPState {
    DeviceState parent_obj;

    uint8_t rregs[ESP_REGS];
    uint8_t wregs[ESP_REGS];
    qemu_irq irq;
    qemu_irq irq_data;
    uint8_t chip_id;
    bool tchi_written;
    int32_t ti_size;
    uint32_t status;
    uint32_t dma;
    Fifo8 fifo;
    SCSIBus bus;
    SCSIDevice *current_dev;
    SCSIRequest *current_req;
    Fifo8 cmdfifo;
    uint8_t cmdfifo_cdb_offset;
    uint8_t lun;
    uint32_t do_cmd;

    bool data_in_ready;
    uint8_t ti_cmd;
    int dma_enabled;

    uint32_t async_len;
    uint8_t *async_buf;

    ESPDMAMemoryReadWriteFunc dma_memory_read;
    ESPDMAMemoryReadWriteFunc dma_memory_write;
    void *dma_opaque;
    void (*dma_cb)(ESPState *s);
    uint8_t pdma_cb;

    uint8_t mig_version_id;

    /* Legacy fields for vmstate_esp version < 5 */
    uint32_t mig_dma_left;
    uint32_t mig_deferred_status;
    bool mig_deferred_complete;
    uint32_t mig_ti_rptr, mig_ti_wptr;
    uint8_t mig_ti_buf[ESP_FIFO_SZ];
    uint8_t mig_cmdbuf[ESP_CMDFIFO_SZ];
    uint32_t mig_cmdlen;
};

#define TYPE_SYSBUS_ESP "sysbus-esp"
OBJECT_DECLARE_SIMPLE_TYPE(SysBusESPState, SYSBUS_ESP)

struct SysBusESPState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    MemoryRegion pdma;
    uint32_t it_shift;
    ESPState esp;
};

#define ESP_TCLO   0x0
#define ESP_TCMID  0x1
#define ESP_FIFO   0x2
#define ESP_CMD    0x3
#define ESP_RSTAT  0x4
#define ESP_WBUSID 0x4
#define ESP_RINTR  0x5
#define ESP_WSEL   0x5
#define ESP_RSEQ   0x6
#define ESP_WSYNTP 0x6
#define ESP_RFLAGS 0x7
#define ESP_WSYNO  0x7
#define ESP_CFG1   0x8
#define ESP_RRES1  0x9
#define ESP_WCCF   0x9
#define ESP_RRES2  0xa
#define ESP_WTEST  0xa
#define ESP_CFG2   0xb
#define ESP_CFG3   0xc
#define ESP_RES3   0xd
#define ESP_TCHI   0xe
#define ESP_RES4   0xf

#define CMD_DMA 0x80
#define CMD_CMD 0x7f

#define CMD_NOP      0x00
#define CMD_FLUSH    0x01
#define CMD_RESET    0x02
#define CMD_BUSRESET 0x03
#define CMD_TI       0x10
#define CMD_ICCS     0x11
#define CMD_MSGACC   0x12
#define CMD_PAD      0x18
#define CMD_SATN     0x1a
#define CMD_RSTATN   0x1b
#define CMD_SEL      0x41
#define CMD_SELATN   0x42
#define CMD_SELATNS  0x43
#define CMD_ENSEL    0x44
#define CMD_DISSEL   0x45

#define STAT_DO 0x00
#define STAT_DI 0x01
#define STAT_CD 0x02
#define STAT_ST 0x03
#define STAT_MO 0x06
#define STAT_MI 0x07
#define STAT_PIO_MASK 0x06

#define STAT_TC 0x10
#define STAT_PE 0x20
#define STAT_GE 0x40
#define STAT_INT 0x80

#define BUSID_DID 0x07

#define INTR_FC 0x08
#define INTR_BS 0x10
#define INTR_DC 0x20
#define INTR_RST 0x80

#define SEQ_0 0x0
#define SEQ_MO 0x1
#define SEQ_CD 0x4

#define CFG1_RESREPT 0x40

#define TCHI_FAS100A 0x4
#define TCHI_AM53C974 0x12

/* PDMA callbacks */
enum pdma_cb {
    SATN_PDMA_CB = 0,
    S_WITHOUT_SATN_PDMA_CB = 1,
    SATN_STOP_PDMA_CB = 2,
    WRITE_RESPONSE_PDMA_CB = 3,
    DO_DMA_PDMA_CB = 4
};

void esp_dma_enable(ESPState *s, int irq, int level);
void esp_request_cancelled(SCSIRequest *req);
void esp_command_complete(SCSIRequest *req, size_t resid);
void esp_transfer_data(SCSIRequest *req, uint32_t len);
void esp_hard_reset(ESPState *s);
uint64_t esp_reg_read(ESPState *s, uint32_t saddr);
void esp_reg_write(ESPState *s, uint32_t saddr, uint64_t val);
extern const VMStateDescription vmstate_esp;
int esp_pre_save(void *opaque);

#endif
