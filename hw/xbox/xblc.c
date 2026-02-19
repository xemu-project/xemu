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

static const uint16_t xblc_sample_rates[5] = { 8000, 11025, 16000, 22050,
                                               24000 };

typedef struct USBXBLCState {
    USBDevice dev;
    uint8_t device_index;
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
        .idVendor          = 0x045e,
        .idProduct         = 0x0283,
        .bcdDevice         = 0x0110,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device,
    .str  = desc_strings,
};

static void usb_xblc_handle_reset(USBDevice *dev)
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

static void xblc_audio_stream_set_rate(USBDevice *dev, uint16_t sample_rate)
{
    USBXBLCState *s = USB_XBLC(dev);

    s->sample_rate = sample_rate;

    SDL_AudioSpec spec = { .channels = 1,
                           .freq = sample_rate,
                           .format = SDL_AUDIO_S16LE };
    if (s->in != NULL) {
        SDL_SetAudioStreamFormat(s->in, &spec, &spec);
    }
    if (s->out != NULL) {
        SDL_SetAudioStreamFormat(s->out, &spec, &spec);
    }
}

static void usb_xblc_handle_control(USBDevice *dev, USBPacket *p, int request,
                                    int value, int index, int length,
                                    uint8_t *data)
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
            uint8_t rate = value & 0xFF;
            assert(rate < ARRAY_SIZE(xblc_sample_rates));
            DPRINTF("Set Sample Rate to %04x", rate);
            xblc_audio_stream_set_rate(dev, xblc_sample_rates[rate]);
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

static void usb_xblc_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXBLCState *s = USB_XBLC(dev);
    uint32_t to_process;
    int32_t chunk_len;
    int32_t available;
    int32_t max_queued_data;
    int32_t copied;
    uint8_t packet[XBLC_MAX_PACKET];

    switch (p->pid) {
    case USB_TOKEN_IN:
        assert(p->ep->nr == XBLC_EP_IN);
        chunk_len = 0;

        if (s->in == NULL) {
            DPRINTF("Tried to get data from the input audio tream but "
                    "the audio stream is not initialized");
            break;
        }

        available = SDL_GetAudioStreamAvailable(s->in);
        if (available < 0) {
            DPRINTF("SDL_GetAudioStreamAvailable Error: %s", SDL_GetError());
            break;
        } else {
            DPRINTF("There are %d bytes of data in the stream", available);
            max_queued_data = s->sample_rate * XBLC_BYTES_PER_SAMPLE *
                              XBLC_QUEUE_SIZE_MS / 1000;
            if (available > max_queued_data) {
                DPRINTF("More than %d bytes of data in the queue. "
                        "Clearing out old data",
                        max_queued_data);
                SDL_ClearAudioStream(s->in);
                available = 0;
            }
        }

        to_process = MIN(p->iov.size, available);
        copied = 0;
        while (copied < to_process) {
            chunk_len = SDL_GetAudioStreamData(s->in, packet,
                                               MIN(sizeof(packet), to_process));
            if (chunk_len < 0) {
                DPRINTF("Error getting data from the input stream: %s",
                        SDL_GetError());
                break;
            }
            usb_packet_copy(p, (void *)packet, chunk_len);
            copied += chunk_len;
        }

        to_process = p->iov.size - copied;
        if (to_process > 0) {
            usb_packet_skip(p, to_process);
        }

        break;
    case USB_TOKEN_OUT:
        assert(p->ep->nr == XBLC_EP_OUT);

        if (s->out == NULL) {
            DPRINTF("Tried to put data into the speaker audio stream "
                    "but the audio stream is not initialized");
            return;
        }

        if (!SDL_PutAudioStreamData(s->out, p->iov.iov->iov_base,
                                    p->iov.size)) {
            DPRINTF("Error putting data into output stream: %s",
                    SDL_GetError());
        }

        break;
    default:
        assert(!"Iso cannot report STALL/HALT");
        break;
    }
}

static void xblc_audio_channel_init(USBXBLCState *s, bool capture, Error **errp)
{
    SDL_AudioStream **channel = capture ? &s->in : &s->out;
    SDL_AudioDeviceID devid = capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING :
                                        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    SDL_AudioSpec spec = { .channels = 1,
                           .freq = s->sample_rate,
                           .format = SDL_AUDIO_S16LE };

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

static void usb_xbox_communicator_realize(USBDevice *dev, Error **errp)
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

static void usb_xbox_communicator_unrealize(USBDevice *dev)
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

static void usb_xblc_class_init(ObjectClass *klass, const void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    uc->handle_reset = usb_xblc_handle_reset;
    uc->handle_control = usb_xblc_handle_control;
    uc->handle_data = usb_xblc_handle_data;
    uc->handle_attach = usb_desc_attach;
}

static const Property xblc_properties[] = {
    DEFINE_PROP_UINT8("index", USBXBLCState, device_index, 0),
};

static const VMStateDescription usb_xblc_vmstate = {
    .name = TYPE_USB_XBLC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_USB_DEVICE(dev, USBXBLCState),
                                // FIXME
                                VMSTATE_END_OF_LIST() },
};

static void usb_xbox_communicator_class_init(ObjectClass *klass,
                                             const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc = XBLC_STR;
    uc->usb_desc = &desc_xblc;
    uc->realize = usb_xbox_communicator_realize;
    uc->unrealize = usb_xbox_communicator_unrealize;
    usb_xblc_class_init(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd = &usb_xblc_vmstate;
    device_class_set_props(dc, xblc_properties);
    dc->desc = XBLC_STR;
}

static const TypeInfo info_xblc = {
    .name = TYPE_USB_XBLC,
    .parent = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXBLCState),
    .class_init = usb_xbox_communicator_class_init,
};

static void usb_xblc_register_types(void)
{
    type_register_static(&info_xblc);
    audio_register_model("xblc", XBLC_STR, TYPE_USB_XBLC);
}

type_init(usb_xblc_register_types)
