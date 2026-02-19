/*
 * QEMU USB Xbox Live Communicator (XBLC) Device
 *
 * Copyright (c) 2022 Ryan Wendland
 * Copyright (c) 2025 Fred Hallock
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/audio/model.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "system/system.h"
#include "hw/hw.h"
#include "ui/console.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "ui/xemu-input.h"
#include "qemu/audio.h"
#include "qemu/fifo8.h"

// #define DEBUG_XBLC
#ifdef DEBUG_XBLC
#define DPRINTF(fmt, ...) printf("[XBLC] " fmt "\n", ##__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define TYPE_USB_XBLC "usb-xblc"
#define USB_XBLC(obj) OBJECT_CHECK(USBXBLCState, (obj), TYPE_USB_XBLC)

#define XBLC_VENDOR_ID 0x045e
#define XBLC_PRODUCT_ID 0x0283
#define XBLC_DEVICE_VERSION 0x0110

#define XBLC_STR "Microsoft Xbox Live Communicator"
#define XBLC_INTERFACE_CLASS 0x78
#define XBLC_INTERFACE_SUBCLASS 0x00
#define XBLC_EP_OUT 0x04
#define XBLC_EP_IN 0x05

#define XBLC_SET_SAMPLE_RATE 0x00
#define XBLC_SET_AGC 0x01

#define XBLC_MAX_PACKET 48
#define XBLC_QUEUE_SIZE_MS 100 /* 100 ms */
#define XBLC_BYTES_PER_SAMPLE 2 /* 16-bit */

// According to Ryzee119, the XBLC appears to default to 16KHz
// https://github.com/Ryzee119/hawk/blob/5ab6f63b280425edd9ee121f5b5520dd8e891990/src/usbd/xblc.c#L143
#define XBLC_DEFAULT_SAMPLE_RATE 16000

typedef struct USBXBLCState {
    USBDevice dev;
    uint8_t auto_gain_control;
    uint16_t sample_rate;

    SDL_AudioStream *in;
    SDL_AudioStream *out;
} USBXBLCState;

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "xemu",
    [STR_PRODUCT] = XBLC_STR,
    [STR_SERIALNUMBER] = "1",
};

static const USBDescIface desc_iface[] = {
    {
        .bInterfaceNumber = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = XBLC_INTERFACE_CLASS,
        .bInterfaceSubClass = XBLC_INTERFACE_SUBCLASS,
        .bInterfaceProtocol = 0x00,
        .eps = (USBDescEndpoint[]){ {
            .bEndpointAddress = USB_DIR_OUT | XBLC_EP_OUT,
            .bmAttributes = USB_ENDPOINT_XFER_ISOC,
            .wMaxPacketSize = XBLC_MAX_PACKET,
            .is_audio = 1,
            .bInterval = 1,
            .bRefresh = 0,
            .bSynchAddress = 0,
        } },
    },
    {
        .bInterfaceNumber = 1,
        .bNumEndpoints = 1,
        .bInterfaceClass = XBLC_INTERFACE_CLASS,
        .bInterfaceSubClass = XBLC_INTERFACE_SUBCLASS,
        .bInterfaceProtocol = 0x00,
        .eps = (USBDescEndpoint[]){ {
            .bEndpointAddress = USB_DIR_IN | XBLC_EP_IN,
            .bmAttributes = USB_ENDPOINT_XFER_ISOC,
            .wMaxPacketSize = XBLC_MAX_PACKET,
            .is_audio = 1,
            .bInterval = 1,
            .bRefresh = 0,
            .bSynchAddress = 0,
        } },
    }
};

static const USBDescDevice desc_device = {
    .bcdUSB = 0x0110,
    .bMaxPacketSize0 = 8,
    .bNumConfigurations = 1,
    .confs =
        (USBDescConfig[]){
            {
                .bNumInterfaces = 2,
                .bConfigurationValue = 1,
                .bmAttributes = USB_CFG_ATT_ONE,
                .bMaxPower = 100,
                .nif = ARRAY_SIZE(desc_iface),
                .ifs = desc_iface,
            },
        },
};

static const USBDesc desc_xblc = {
    .id = {
        .idVendor = XBLC_VENDOR_ID,
        .idProduct = XBLC_PRODUCT_ID,
        .bcdDevice = XBLC_DEVICE_VERSION,
        .iManufacturer = STR_MANUFACTURER,
        .iProduct = STR_PRODUCT,
        .iSerialNumber = STR_SERIALNUMBER,
    },
    .full = &desc_device,
    .str = desc_strings,
};

static int xblc_get_sample_rate_for_index(unsigned int index)
{
    static const uint16_t sample_rates[] = { 8000, 11025, 16000, 22050, 24000 };
    assert(index < ARRAY_SIZE(sample_rates));
    return sample_rates[index];
}

static SDL_AudioSpec xblc_get_audio_spec(USBXBLCState *s)
{
    SDL_AudioSpec spec = { .channels = 1,
                           .freq = s->sample_rate,
                           .format = SDL_AUDIO_S16LE };
    return spec;
}

static void xblc_handle_reset(USBDevice *dev)
{
    USBXBLCState *s = USB_XBLC(dev);

    DPRINTF("Reset");

    if (s->in != NULL) {
        SDL_ClearAudioStream(s->in);
    }
    if (s->out != NULL) {
        SDL_ClearAudioStream(s->out);
    }
}

static void xblc_audio_channel_update_format(USBXBLCState *s)
{
    SDL_AudioSpec spec = xblc_get_audio_spec(s);

    if (s->in != NULL) {
        SDL_SetAudioStreamFormat(s->in, &spec, &spec);
    }
    if (s->out != NULL) {
        SDL_SetAudioStreamFormat(s->out, &spec, &spec);
    }
}

static void xblc_set_sample_rate(USBXBLCState *s, uint16_t sample_rate)
{
    s->sample_rate = sample_rate;
    xblc_audio_channel_update_format(s);
}

static void xblc_handle_control(USBDevice *dev, USBPacket *p, int request,
                                int value, int index, int length, uint8_t *data)
{
    USBXBLCState *s = USB_XBLC(dev);

    if (usb_desc_handle_control(dev, p, request, value, index, length, data) >=
        0) {
        DPRINTF("USB Control request handled by usb_desc_handle_control");
        return;
    }

    switch (request) {
    case VendorInterfaceOutRequest | USB_REQ_SET_FEATURE:
        if (index == XBLC_SET_SAMPLE_RATE) {
            xblc_set_sample_rate(s,
                                 xblc_get_sample_rate_for_index(value & 0xFF));
            break;
        } else if (index == XBLC_SET_AGC) {
            DPRINTF("Set Auto Gain Control to %d", value);
            s->auto_gain_control = (value) ? 1 : 0;
            break;
        }
        // Fallthrough
    default:
        DPRINTF("USB stalled on request 0x%x value 0x%x", request, value);
        p->status = USB_RET_STALL;
        assert(!"USB stalled on request");
        return;
    }
}

static void xblc_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXBLCState *s = USB_XBLC(dev);

    switch (p->pid) {
    case USB_TOKEN_IN: {
        assert(p->ep->nr == XBLC_EP_IN);

        if (s->in == NULL) {
            break;
        }

        int available = SDL_GetAudioStreamAvailable(s->in);
        if (available < 0) {
            DPRINTF("SDL_GetAudioStreamAvailable failed: %s", SDL_GetError());
            break;
        }

        int max_queued_data =
            s->sample_rate * XBLC_BYTES_PER_SAMPLE * XBLC_QUEUE_SIZE_MS / 1000;
        if (available > max_queued_data) {
            DPRINTF("Available data exceeded max threshold. Clearing stream.");
            SDL_ClearAudioStream(s->in);
            available = 0;
        }

        for (size_t remaining = MIN(p->iov.size, available); remaining > 0;) {
            uint8_t packet[XBLC_MAX_PACKET];

            int chunk_len = SDL_GetAudioStreamData(
                s->in, packet, MIN(sizeof(packet), remaining));
            if (chunk_len < 0) {
                DPRINTF("SDL_GetAudioStreamData failed: %s", SDL_GetError());
                break;
            } else if (chunk_len > 0) {
                usb_packet_copy(p, (void *)packet, chunk_len);
                remaining -= chunk_len;
            }
        }

        break;
    }
    case USB_TOKEN_OUT:
        assert(p->ep->nr == XBLC_EP_OUT);
        if (s->out != NULL) {
            if (!SDL_PutAudioStreamData(s->out, p->iov.iov->iov_base,
                                        p->iov.size)) {
                DPRINTF("SDL_PutAudioStreamData failed: %s", SDL_GetError());
            }
        }
        break;
    default:
        assert(!"Iso cannot report STALL/HALT");
        break;
    }

    if (p->pid == USB_TOKEN_IN && p->iov.size > p->actual_length) {
        usb_packet_skip(p, p->iov.size - p->actual_length);
    }
}

static void xblc_audio_channel_init(USBXBLCState *s, bool capture, Error **errp)
{
    SDL_AudioStream **channel = capture ? &s->in : &s->out;
    SDL_AudioDeviceID devid = capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING :
                                        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    SDL_AudioSpec spec = xblc_get_audio_spec(s);

    if (*channel != NULL) {
        SDL_DestroyAudioStream(*channel);
    }
    *channel = SDL_OpenAudioDeviceStream(devid, &spec, NULL, (void *)s);
    if (*channel == NULL) {
        error_setg(errp, "Failed to open audio device stream: %s",
                   SDL_GetError());
        return;
    }

    SDL_ResumeAudioStreamDevice(*channel);
}

static void xblc_realize(USBDevice *dev, Error **errp)
{
    USBXBLCState *s = USB_XBLC(dev);
    Error *err = NULL;

    usb_desc_create_serial(dev);
    usb_desc_init(dev);

    s->in = NULL;
    s->out = NULL;
    s->sample_rate = XBLC_DEFAULT_SAMPLE_RATE;

    xblc_audio_channel_init(s, true, &err);
    if (err) {
        warn_report_err(err);
    }

    xblc_audio_channel_init(s, false, &err);
    if (err) {
        warn_report_err(err);
    }
}

static void xblc_unrealize(USBDevice *dev)
{
    USBXBLCState *s = USB_XBLC(dev);

    if (s->in) {
        SDL_DestroyAudioStream(s->in);
        s->in = NULL;
    }

    if (s->out) {
        SDL_DestroyAudioStream(s->out);
        s->out = NULL;
    }
}

static int xblc_post_load(void *opaque, int version_id)
{
    USBXBLCState *s = USB_XBLC(opaque);

    xblc_audio_channel_update_format(s);

    return 0;
}

static const VMStateDescription xblc_vmstate = {
    .name = TYPE_USB_XBLC,
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = xblc_post_load,
    .fields = (VMStateField[]){ VMSTATE_USB_DEVICE(dev, USBXBLCState),
                                VMSTATE_UINT16(sample_rate, USBXBLCState),
                                VMSTATE_END_OF_LIST() },
};

static void xblc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc = XBLC_STR;
    uc->usb_desc = &desc_xblc;
    uc->realize = xblc_realize;
    uc->unrealize = xblc_unrealize;
    uc->handle_reset = xblc_handle_reset;
    uc->handle_control = xblc_handle_control;
    uc->handle_data = xblc_handle_data;
    uc->handle_attach = usb_desc_attach;

    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd = &xblc_vmstate;
    dc->desc = XBLC_STR;
}

static const TypeInfo info_xblc = {
    .name = TYPE_USB_XBLC,
    .parent = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXBLCState),
    .class_init = xblc_class_init,
};

static void xblc_register_types(void)
{
    type_register_static(&info_xblc);
    audio_register_model("xblc", XBLC_STR, TYPE_USB_XBLC);
}

type_init(xblc_register_types)
