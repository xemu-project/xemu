#include "xemu-xbe.h"
#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "monitor/hmp-target.h"
#include "sysemu/hw_accel.h"

// #include "base64/base64.h"
// #include "base64/base64.c"

static int virt_to_phys(target_ulong virt_addr, hwaddr *phys_addr)
{
    MemTxAttrs attrs;
    CPUState *cs;
    hwaddr gpa;

    cs = qemu_get_cpu(0);
    if (!cs) {
        return 1; // No cpu
    }

    cpu_synchronize_state(cs);

    gpa = cpu_get_phys_page_attrs_debug(cs, virt_addr & TARGET_PAGE_MASK, &attrs);
    if (gpa == -1) {
        return 1; // Unmapped
    } else {
        *phys_addr = gpa + (virt_addr & ~TARGET_PAGE_MASK);
    }

    return 0;
}

static ssize_t virt_dma_memory_read(vaddr vaddr, void *buf, size_t len)
{
    size_t num_bytes_read = 0;

    while (num_bytes_read < len) {
        // Get physical page for this offset
        hwaddr phys_addr = 0;
        if (virt_to_phys(vaddr+num_bytes_read, &phys_addr) != 0) {
            return -1;
        }

        // Read contents from the page
        size_t bytes_in_chunk = (len-num_bytes_read) & (TARGET_PAGE_SIZE-1);
        // FIXME: Check return value
        dma_memory_read(&address_space_memory, phys_addr, buf + num_bytes_read, bytes_in_chunk);

        num_bytes_read += bytes_in_chunk;
    }

    return num_bytes_read;
}

// Get current XBE info
struct xbe_info *xemu_get_xbe_info(void)
{
    vaddr xbe_hdr_addr_virt = 0x10000;

    static struct xbe_info xbe_info;

    if (xbe_info.headers) {
        free(xbe_info.headers);
        xbe_info.headers = NULL;
    }

    // Get physical page of headers
    hwaddr hdr_addr_phys = 0;
    if (virt_to_phys(xbe_hdr_addr_virt, &hdr_addr_phys) != 0) {
        return NULL;
    }

    // Check `XBEH` signature
    uint32_t sig = ldl_le_phys(&address_space_memory, hdr_addr_phys);
    if (sig != 0x48454258) {
        return NULL;
    }

    // Determine full length of headers
    xbe_info.headers_len = ldl_le_phys(&address_space_memory,
        hdr_addr_phys + offsetof(struct xbe_header, m_sizeof_headers));
    if (xbe_info.headers_len > 4*TARGET_PAGE_SIZE) {
        // Headers are unusually large
        return NULL;
    }

    xbe_info.headers = malloc(xbe_info.headers_len);
    assert(xbe_info.headers != NULL);
    
    // Read all XBE headers
    ssize_t bytes_read = virt_dma_memory_read(xbe_hdr_addr_virt,
                                              xbe_info.headers,
                                              xbe_info.headers_len);
    if (bytes_read != xbe_info.headers_len) {
        // Failed to read headers
        return NULL;
    }

    // Extract XBE header fields
    xbe_info.xbe_hdr = (struct xbe_header *)xbe_info.headers;
    xbe_info.timedate = ldl_le_p(&xbe_info.xbe_hdr->m_timedate);

    // Get certificate
    uint32_t cert_addr_virt = ldl_le_p(&xbe_info.xbe_hdr->m_certificate_addr);
    if ((cert_addr_virt == 0) || ((cert_addr_virt + sizeof(struct xbe_certificate)) > (xbe_hdr_addr_virt + xbe_info.headers_len))) {
        // Invalid certificate header (a valid certificate is expected for official titles)
        return NULL;
    }
    
    // Extract certificate fields
    xbe_info.xbe_cert = (struct xbe_certificate *)(xbe_info.headers + cert_addr_virt - xbe_hdr_addr_virt);
    xbe_info.cert_timedate = ldl_le_p(&xbe_info.xbe_cert->m_timedate);
    xbe_info.cert_title_id = ldl_le_p(&xbe_info.xbe_cert->m_titleid);
    xbe_info.cert_version = ldl_le_p(&xbe_info.xbe_cert->m_version);
    xbe_info.cert_region = ldl_le_p(&xbe_info.xbe_cert->m_game_region);
    xbe_info.cert_disc_num = ldl_le_p(&xbe_info.xbe_cert->m_disk_number);

#if 0
    // Dump base64 version of headers
    FILE *fd = fopen("dump2.bin", "wb");
    void *b64buf = malloc(xbe_info.headers_len*2);
    int enc = base64_encode(xbe_info.headers, xbe_info.headers_len, b64buf);
    fwrite(b64buf, 1, enc, fd);
    free(b64buf);
#endif

    return &xbe_info;
}
