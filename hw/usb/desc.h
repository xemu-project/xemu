#ifndef QEMU_HW_USB_DESC_H
#define QEMU_HW_USB_DESC_H

#include <wchar.h>

/* binary representation */
typedef struct USBDescriptor {
    uint8_t                   bLength;
    uint8_t                   bDescriptorType;
    union {
        struct {
            uint8_t           bcdUSB_lo;
            uint8_t           bcdUSB_hi;
            uint8_t           bDeviceClass;
            uint8_t           bDeviceSubClass;
            uint8_t           bDeviceProtocol;
            uint8_t           bMaxPacketSize0;
            uint8_t           idVendor_lo;
            uint8_t           idVendor_hi;
            uint8_t           idProduct_lo;
            uint8_t           idProduct_hi;
            uint8_t           bcdDevice_lo;
            uint8_t           bcdDevice_hi;
            uint8_t           iManufacturer;
            uint8_t           iProduct;
            uint8_t           iSerialNumber;
            uint8_t           bNumConfigurations;
        } device;
        struct {
            uint8_t           bcdUSB_lo;
            uint8_t           bcdUSB_hi;
            uint8_t           bDeviceClass;
            uint8_t           bDeviceSubClass;
            uint8_t           bDeviceProtocol;
            uint8_t           bMaxPacketSize0;
            uint8_t           bNumConfigurations;
            uint8_t           bReserved;
        } device_qualifier;
        struct {
            uint8_t           wTotalLength_lo;
            uint8_t           wTotalLength_hi;
            uint8_t           bNumInterfaces;
            uint8_t           bConfigurationValue;
            uint8_t           iConfiguration;
            uint8_t           bmAttributes;
            uint8_t           bMaxPower;
        } config;
        struct {
            uint8_t           bInterfaceNumber;
            uint8_t           bAlternateSetting;
            uint8_t           bNumEndpoints;
            uint8_t           bInterfaceClass;
            uint8_t           bInterfaceSubClass;
            uint8_t           bInterfaceProtocol;
            uint8_t           iInterface;
        } interface;
        struct {
            uint8_t           bEndpointAddress;
            uint8_t           bmAttributes;
            uint8_t           wMaxPacketSize_lo;
            uint8_t           wMaxPacketSize_hi;
            uint8_t           bInterval;
            uint8_t           bRefresh;        /* only audio ep */
            uint8_t           bSynchAddress;   /* only audio ep */
        } endpoint;
        struct {
            uint8_t           bMaxBurst;
            uint8_t           bmAttributes;
            uint8_t           wBytesPerInterval_lo;
            uint8_t           wBytesPerInterval_hi;
        } super_endpoint;
        struct {
            uint8_t           wTotalLength_lo;
            uint8_t           wTotalLength_hi;
            uint8_t           bNumDeviceCaps;
        } bos;
        struct {
            uint8_t           bDevCapabilityType;
            union {
                struct {
                    uint8_t   bmAttributes_1;
                    uint8_t   bmAttributes_2;
                    uint8_t   bmAttributes_3;
                    uint8_t   bmAttributes_4;
                } usb2_ext;
                struct {
                    uint8_t   bmAttributes;
                    uint8_t   wSpeedsSupported_lo;
                    uint8_t   wSpeedsSupported_hi;
                    uint8_t   bFunctionalitySupport;
                    uint8_t   bU1DevExitLat;
                    uint8_t   wU2DevExitLat_lo;
                    uint8_t   wU2DevExitLat_hi;
                } super;
            } u;
        } cap;
    } u;
} QEMU_PACKED USBDescriptor;

struct USBDescID {
    uint16_t                  idVendor;
    uint16_t                  idProduct;
    uint16_t                  bcdDevice;
    uint8_t                   iManufacturer;
    uint8_t                   iProduct;
    uint8_t                   iSerialNumber;
};

struct USBDescDevice {
    uint16_t                  bcdUSB;
    uint8_t                   bDeviceClass;
    uint8_t                   bDeviceSubClass;
    uint8_t                   bDeviceProtocol;
    uint8_t                   bMaxPacketSize0;
    uint8_t                   bNumConfigurations;

    const USBDescConfig       *confs;
};

struct USBDescConfig {
    uint8_t                   bNumInterfaces;
    uint8_t                   bConfigurationValue;
    uint8_t                   iConfiguration;
    uint8_t                   bmAttributes;
    uint8_t                   bMaxPower;

    /* grouped interfaces */
    uint8_t                   nif_groups;
    const USBDescIfaceAssoc   *if_groups;

    /* "normal" interfaces */
    uint8_t                   nif;
    const USBDescIface        *ifs;
};

/* conceptually an Interface Association Descriptor, and related interfaces */
struct USBDescIfaceAssoc {
    uint8_t                   bFirstInterface;
    uint8_t                   bInterfaceCount;
    uint8_t                   bFunctionClass;
    uint8_t                   bFunctionSubClass;
    uint8_t                   bFunctionProtocol;
    uint8_t                   iFunction;

    uint8_t                   nif;
    const USBDescIface        *ifs;
};

struct USBDescIface {
    uint8_t                   bInterfaceNumber;
    uint8_t                   bAlternateSetting;
    uint8_t                   bNumEndpoints;
    uint8_t                   bInterfaceClass;
    uint8_t                   bInterfaceSubClass;
    uint8_t                   bInterfaceProtocol;
    uint8_t                   iInterface;

    uint8_t                   ndesc;
    USBDescOther              *descs;
    USBDescEndpoint           *eps;
};

struct USBDescEndpoint {
    uint8_t                   bEndpointAddress;
    uint8_t                   bmAttributes;
    uint16_t                  wMaxPacketSize;
    uint8_t                   bInterval;
    uint8_t                   bRefresh;
    uint8_t                   bSynchAddress;

    uint8_t                   is_audio; /* has bRefresh + bSynchAddress */
    uint8_t                   *extra;

    /* superspeed endpoint companion */
    uint8_t                   bMaxBurst;
    uint8_t                   bmAttributes_super;
    uint16_t                  wBytesPerInterval;
};

struct USBDescOther {
    uint8_t                   length;
    const uint8_t             *data;
};

struct USBDescMSOS {
    const char                *CompatibleID;
    const wchar_t             *Label;
    bool                      SelectiveSuspendEnabled;
};

typedef const char *USBDescStrings[256];

struct USBDesc {
    USBDescID                 id;
    const USBDescDevice       *full;
    const USBDescDevice       *high;
    const USBDescDevice       *super;
    const char* const         *str;
    const USBDescMSOS         *msos;
};

#define USB_DESC_MAX_LEN    8192
#define USB_DESC_FLAG_SUPER (1 << 1)

/* little helpers */
static inline uint8_t usb_lo(uint16_t val)
{
    return val & 0xff;
}

static inline uint8_t usb_hi(uint16_t val)
{
    return (val >> 8) & 0xff;
}

/* generate usb packages from structs */
int usb_desc_device(const USBDescID *id, const USBDescDevice *dev,
                    bool msos, uint8_t *dest, size_t len);
int usb_desc_device_qualifier(const USBDescDevice *dev,
                              uint8_t *dest, size_t len);
int usb_desc_config(const USBDescConfig *conf, int flags,
                    uint8_t *dest, size_t len);
int usb_desc_iface_group(const USBDescIfaceAssoc *iad, int flags,
                         uint8_t *dest, size_t len);
int usb_desc_iface(const USBDescIface *iface, int flags,
                   uint8_t *dest, size_t len);
int usb_desc_endpoint(const USBDescEndpoint *ep, int flags,
                      uint8_t *dest, size_t len);
int usb_desc_other(const USBDescOther *desc, uint8_t *dest, size_t len);
int usb_desc_msos(const USBDesc *desc, USBPacket *p,
                  int index, uint8_t *dest, size_t len);

/* control message emulation helpers */
void usb_desc_init(USBDevice *dev);
void usb_desc_attach(USBDevice *dev);
void usb_desc_set_string(USBDevice *dev, uint8_t index, const char *str);
void usb_desc_create_serial(USBDevice *dev);
const char *usb_desc_get_string(USBDevice *dev, uint8_t index);
int usb_desc_string(USBDevice *dev, int index, uint8_t *dest, size_t len);
int usb_desc_get_descriptor(USBDevice *dev, USBPacket *p,
        int value, uint8_t *dest, size_t len);
int usb_desc_handle_control(USBDevice *dev, USBPacket *p,
        int request, int value, int index, int length, uint8_t *data);

#endif /* QEMU_HW_USB_DESC_H */
