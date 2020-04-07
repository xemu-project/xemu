#include "xemu-xbe.h"
#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "monitor/hmp-target.h"
#include "sysemu/hw_accel.h"

#if 0
// http://www.caustik.com/cxbx/download/xbe.htm
struct xbe_header
{
    uint32_t m_magic;                         // magic number [should be "XBEH"]
    uint8_t  m_digsig[256];                   // digital signature
    uint32_t m_base;                          // base address
    uint32_t m_sizeof_headers;                // size of headers
    uint32_t m_sizeof_image;                  // size of image
    uint32_t m_sizeof_image_header;           // size of image header
    uint32_t m_timedate;                      // timedate stamp
    uint32_t m_certificate_addr;              // certificate address
    uint32_t m_sections;                      // number of sections
    uint32_t m_section_headers_addr;          // section headers address

    struct init_flags
    {
        uint32_t m_mount_utility_drive    : 1;  // mount utility drive flag
        uint32_t m_format_utility_drive   : 1;  // format utility drive flag
        uint32_t m_limit_64mb             : 1;  // limit development kit run time memory to 64mb flag
        uint32_t m_dont_setup_harddisk    : 1;  // don't setup hard disk flag
        uint32_t m_unused                 : 4;  // unused (or unknown)
        uint32_t m_unused_b1              : 8;  // unused (or unknown)
        uint32_t m_unused_b2              : 8;  // unused (or unknown)
        uint32_t m_unused_b3              : 8;  // unused (or unknown)
    } m_init_flags;

    uint32_t m_entry;                         // entry point address
    uint32_t m_tls_addr;                      // thread local storage directory address
    uint32_t m_pe_stack_commit;               // size of stack commit
    uint32_t m_pe_heap_reserve;               // size of heap reserve
    uint32_t m_pe_heap_commit;                // size of heap commit
    uint32_t m_pe_base_addr;                  // original base address
    uint32_t m_pe_sizeof_image;               // size of original image
    uint32_t m_pe_checksum;                   // original checksum
    uint32_t m_pe_timedate;                   // original timedate stamp
    uint32_t m_debug_pathname_addr;           // debug pathname address
    uint32_t m_debug_filename_addr;           // debug filename address
    uint32_t m_debug_unicode_filename_addr;   // debug unicode filename address
    uint32_t m_kernel_image_thunk_addr;       // kernel image thunk address
    uint32_t m_nonkernel_import_dir_addr;     // non kernel import directory address
    uint32_t m_library_versions;              // number of library versions
    uint32_t m_library_versions_addr;         // library versions address
    uint32_t m_kernel_library_version_addr;   // kernel library version address
    uint32_t m_xapi_library_version_addr;     // xapi library version address
    uint32_t m_logo_bitmap_addr;              // logo bitmap address
    uint32_t m_logo_bitmap_size;              // logo bitmap size
};

struct xbe_certificate
{
    uint32_t m_size;                          // size of certificate
    uint32_t m_timedate;                      // timedate stamp
    uint32_t m_titleid;                       // title id
    uint16_t m_title_name[40];                // title name (unicode)
    uint32_t m_alt_title_id[0x10];            // alternate title ids
    uint32_t m_allowed_media;                 // allowed media types
    uint32_t m_game_region;                   // game region
    uint32_t m_game_ratings;                  // game ratings
    uint32_t m_disk_number;                   // disk number
    uint32_t m_version;                       // version
    uint8_t  m_lan_key[16];                   // lan key
    uint8_t  m_sig_key[16];                   // signature key
    uint8_t  m_title_alt_sig_key[16][16];     // alternate signature keys
};
#endif

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

// Get current XBE info
struct xbe_info *xemu_get_xbe_info(void)
{
    static struct xbe_info xbe_info;
    hwaddr hdr_addr;

    // Get physical page offset of headers
    if (virt_to_phys(0x10000, &hdr_addr) != 0) {
        return NULL;
    }

    // Check signature
    uint32_t sig = ldl_le_phys(&address_space_memory, hdr_addr);
    if (sig != 0x48454258) {
        return NULL;
    }

    xbe_info.timedate = ldl_le_phys(&address_space_memory, hdr_addr+0x114);

    // Find certificate (likely on same page, but be safe and map it)
    uint32_t cert_addr_virt = ldl_le_phys(&address_space_memory, hdr_addr+0x118);
    if (cert_addr_virt == 0) {
        return NULL;
    }

    hwaddr cert_addr;
    if (virt_to_phys(cert_addr_virt, &cert_addr) != 0) {
        return NULL;
    }

    // Extract title info from certificate
    xbe_info.cert_timedate = ldl_le_phys(&address_space_memory, cert_addr+0x04);
    xbe_info.cert_title_id = ldl_le_phys(&address_space_memory, cert_addr+0x08);
    xbe_info.cert_version  = ldl_le_phys(&address_space_memory, cert_addr+0xac);

    // Generate friendly name for title id
    uint8_t pub_hi = xbe_info.cert_title_id >> 24;
    uint8_t pub_lo = xbe_info.cert_title_id >> 16;

    if ((65 > pub_hi) || (pub_hi > 90) || (65 > pub_lo) || (pub_lo > 90)) {
        // Non-printable publisher id
        snprintf(xbe_info.cert_title_id_str, sizeof(xbe_info.cert_title_id_str),
            "0x%08x", xbe_info.cert_title_id);
    } else {
        // Printable publisher id
        snprintf(xbe_info.cert_title_id_str, sizeof(xbe_info.cert_title_id_str),
            "%c%c-%03u", pub_hi, pub_lo, xbe_info.cert_title_id & 0xffff);
    }

    return &xbe_info;
}
