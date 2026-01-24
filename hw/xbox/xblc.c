/*
 * QEMU USB Xbox Live Communicator (XBLC) Device
 *
 * Copyright (c) 2022 Ryan Wendland
 * Copyright (c) 2025 faha223
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
#include "xblc.h"

//#define DEBUG_XBLC
#ifdef DEBUG_XBLC
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

#define TYPE_USB_XBLC "usb-xblc"
#define USB_XBLC(obj) OBJECT_CHECK(USBXBLCState, (obj), TYPE_USB_XBLC)

#define XBLC_STR "Microsoft Xbox Live Communicator"
#define XBLC_INTERFACE_CLASS    0x78
#define XBLC_INTERFACE_SUBCLASS 0x00
#define XBLC_EP_OUT             0x04
#define XBLC_EP_IN              0x05

#define XBLC_SET_SAMPLE_RATE    0x00
#define XBLC_SET_AGC            0x01

#define XBLC_MAX_PACKET 48
#define XBLC_FIFO_SIZE (XBLC_MAX_PACKET * 100) //~100 ms worth of audio at 16bit 24kHz

static const uint8_t silence[256] = {0};

static const uint16_t xblc_sample_rates[5] = {
    8000, 11025, 16000, 22050, 24000
};

typedef struct XBLCStream {
    SDL_AudioDeviceID device_id;
    SDL_AudioStream *voice;
    SDL_AudioSpec spec;
    uint8_t packet[XBLC_MAX_PACKET];
    Fifo8 fifo;
    float volume;
    float peak_volume;
} XBLCStream;

typedef struct USBXBLCState {
    USBDevice dev;
    uint8_t   device_index;
    uint8_t   auto_gain_control;
    uint16_t  sample_rate;

    XBLCStream out;
    XBLCStream in;
} USBXBLCState;

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "xemu",
    [STR_PRODUCT]      = XBLC_STR,
    [STR_SERIALNUMBER] = "1",
};

static const USBDescIface desc_iface[]= {
    {
        .bInterfaceNumber              = 0,
        .bNumEndpoints                 = 1,
        .bInterfaceClass               = XBLC_INTERFACE_CLASS,
        .bInterfaceSubClass            = XBLC_INTERFACE_SUBCLASS,
        .bInterfaceProtocol            = 0x00,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_OUT | XBLC_EP_OUT,
                .bmAttributes          = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize        = XBLC_MAX_PACKET,
                .is_audio              = 1,
                .bInterval             = 1,
                .bRefresh              = 0,
                .bSynchAddress         = 0,
            }
        },
    },
    {
        .bInterfaceNumber              = 1,
        .bNumEndpoints                 = 1,
        .bInterfaceClass               = XBLC_INTERFACE_CLASS,
        .bInterfaceSubClass            = XBLC_INTERFACE_SUBCLASS,
        .bInterfaceProtocol            = 0x00,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | XBLC_EP_IN,
                .bmAttributes          = USB_ENDPOINT_XFER_ISOC,
                .wMaxPacketSize        = XBLC_MAX_PACKET,
                .is_audio              = 1,
                .bInterval             = 1,
                .bRefresh              = 0,
                .bSynchAddress         = 0,
            }
        },
    }
};

static const USBDescDevice desc_device = {
    .bcdUSB                        = 0x0110,
    .bMaxPacketSize0               = 8,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 2,
            .bConfigurationValue   = 1,
            .bmAttributes          = USB_CFG_ATT_ONE,
            .bMaxPower             = 100,
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
    USBXBLCState *s = (USBXBLCState *)dev;

    DPRINTF("[XBLC] Reset\n");

    if(s->in.voice != 0)
        SDL_LockAudioStream(s->in.voice);
    if(s->out.voice != 0)
        SDL_LockAudioStream(s->out.voice);

    fifo8_reset(&s->in.fifo);
    fifo8_reset(&s->out.fifo);

    if(s->in.voice != 0)
        SDL_UnlockAudioStream(s->in.voice);
    if(s->out.voice != 0)
        SDL_UnlockAudioStream(s->out.voice);
}

float xblc_audio_stream_get_current_input_volume(void *dev)
{
    USBXBLCState *s = (USBXBLCState*)dev;
    return s->in.peak_volume / 32768.0f;
}

float xblc_audio_stream_get_output_volume(void *dev)
{
    USBXBLCState *s = (USBXBLCState*)dev;
    return s->out.volume;
}

float xblc_audio_stream_get_input_volume(void *dev)
{
    USBXBLCState *s = (USBXBLCState*)dev;
    return s->in.volume;
}

void xblc_audio_stream_set_output_volume(void *dev, float volume)
{
    USBXBLCState *s = (USBXBLCState*)dev;
    s->out.volume = MIN(1.0, MAX(0, volume));
}
void xblc_audio_stream_set_input_volume(void *dev, float volume)
{
    USBXBLCState *s = (USBXBLCState*)dev;
    s->in.volume = MIN(1.0, MAX(0, volume));
}

static void output_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    static uint8_t mixed[XBLC_FIFO_SIZE];
    USBXBLCState *s = (USBXBLCState *)userdata;
    const uint8_t *data;
    uint32_t max_len;

    // Not enough data to send, wait a bit longer, fill with silence for now
    if (fifo8_num_used(&s->out.fifo) < XBLC_MAX_PACKET) {
        if (!SDL_PutAudioStreamData(stream, (void*)silence, MIN(total_amount, ARRAY_SIZE(silence))))
            DPRINTF("[XBLC] Error putting audio stream data: %s\n", SDL_GetError());
    } else {
        // Write speaker data into audio backend
        while (total_amount > 0 && !fifo8_is_empty(&s->out.fifo)) {
            max_len = MIN(fifo8_num_used(&s->out.fifo), (uint32_t)total_amount);
            data    = fifo8_pop_bufptr(&s->out.fifo, max_len, &max_len);

            if(s->out.volume < 1.0) {
                if (!SDL_MixAudio(mixed, data, SDL_AUDIO_S16LE, max_len, MAX(0, s->out.volume)))
                    DPRINTF("[XBLC] Error mixing audio: %s\n", SDL_GetError());
                else {
                    if (!SDL_PutAudioStreamData(stream, mixed, max_len))
                        DPRINTF("[XBLC] Error getting audio stream data: %s\n", SDL_GetError());
                }
            } else {
                if (!SDL_PutAudioStreamData(stream, data, max_len)) 
                    DPRINTF("[XBLC] Error getting audio stream data: %s\n", SDL_GetError());
            }
            total_amount -= max_len;
        }
    }
}

static int calc_peak_amplitude(const int16_t *samples, int len) {
    int max = 0;
    for(int i = 0; i < len; i++) {
        max = MAX(max, abs(samples[i]));
    }
    return max;
}

static void input_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    DPRINTF("[XBLC] Input Callback: total_amount=%d\n", total_amount);
    USBXBLCState *s = (USBXBLCState *)userdata;
    static uint8_t buffer[XBLC_FIFO_SIZE];
    static uint8_t mixed[XBLC_FIFO_SIZE];
    int peak;
    int64_t scaled_peak;
    
    peak = calc_peak_amplitude((int16_t *)stream, total_amount / sizeof(int16_t));
    scaled_peak = (int64_t)s->in.volume * peak;
    s->in.peak_volume = (int)(scaled_peak);

    // Don't try to put more into the queue than will fit
    uint32_t total_bytes_read = 0;
    uint32_t max_len = MIN(total_amount, fifo8_num_free(&s->in.fifo));
    if (max_len > 0) {
        int bytes_read = SDL_GetAudioStreamData(stream, buffer, max_len);
        if(bytes_read > 0)
        {
            total_bytes_read += bytes_read;
            if(s->in.volume < 1.0) {
                SDL_MixAudio(mixed, buffer, SDL_AUDIO_S16LE, max_len, MAX(s->in.volume, 0));
                fifo8_push_all(&s->in.fifo, mixed, max_len);
            } else {
                fifo8_push_all(&s->in.fifo, buffer, bytes_read);
            }
        } else if (bytes_read < 0) {
            DPRINTF("[XBLC] Error getting audio stream data: %s\n", SDL_GetError());
        }
    }
            
    // Clear out the remainder of the input buffer
    DPRINTF("[XBLC] Input Callback: Clearing Input Stream\n");
    int bytes_read = 1;
    while (bytes_read > 0 && total_bytes_read < total_amount) {
        bytes_read = SDL_GetAudioStreamData(stream, buffer, MIN(XBLC_FIFO_SIZE, total_amount - total_bytes_read));
        if (bytes_read > 0)
            total_bytes_read += bytes_read;
        else if(bytes_read < 0) {
            DPRINTF("[XBLC] Error getting audio stream data: %s\n", SDL_GetError());
            break;
        }
    }
    DPRINTF("[XBLC] Input Callback: Read %d bytes\n", total_bytes_read);
}

static void xblc_audio_channel_init(USBXBLCState *s, bool capture, SDL_AudioDeviceID device_id)
{
    XBLCStream *channel = capture ? &s->in : &s->out;
    
    if(channel->voice != 0) {
        SDL_PauseAudioStreamDevice(channel->voice);
        SDL_DestroyAudioStream(channel->voice);
        channel->voice = 0;
    }

    channel->device_id = device_id;
    
    fifo8_reset(&channel->fifo);
    if(capture)
        s->in.peak_volume = 0;

    channel->spec.channels = 1;
    channel->spec.freq = s->sample_rate;
    channel->spec.format = SDL_AUDIO_S16LE;

    channel->voice = SDL_OpenAudioDeviceStream(device_id,
                                               &channel->spec, 
                                               capture ? input_callback : output_callback,
                                               (void*)s);
    if (channel->voice == NULL) {
        DPRINTF("[XBLC] Failed to open audio device stream: %s\n", SDL_GetError());
        return;
    }
    
    SDL_ResumeAudioStreamDevice(channel->voice);
}

static bool should_init_stream(const XBLCStream *stream, SDL_AudioDeviceID requested_device_id)
{
    // If the voice has not been initialized, initialize it
    if (stream->voice == 0)
        return true;
    
    // If the new id doesn't match the existing id, initialize it
    if (stream->device_id != requested_device_id)
        return true;
    
    // We don't need to initialize it
    return false;
}

static void xblc_audio_stream_init(USBDevice *dev, uint16_t sample_rate)
{
    USBXBLCState *s = (USBXBLCState *)dev;
    bool init_input_stream = false, init_output_stream = false;

    ControllerState *controller = xemu_input_get_bound(s->device_index);
    assert(controller->peripheral_types[0] == PERIPHERAL_XBLC);
    assert(controller->peripherals[0] != NULL);

    XblcState *xblc = (XblcState*)controller->peripherals[0];

    if(s->sample_rate != sample_rate) {
        init_input_stream = true;
        init_output_stream = true;
        s->sample_rate = sample_rate;
    }

    init_input_stream |= should_init_stream(&s->in, xblc->input_device_id);
    init_output_stream |= should_init_stream(&s->out, xblc->output_device_id);

    if (init_input_stream) {
        xblc_audio_channel_init(s, true, xblc->input_device_id);
    }
    if (init_output_stream) {
        xblc_audio_channel_init(s, false, xblc->output_device_id);
    }

    DPRINTF("[XBLC] Init audio streams at %d Hz\n", sample_rate);
}

void xblc_audio_stream_reinit(void *dev)
{
    USBXBLCState *s = (USBXBLCState *)dev;
    xblc_audio_stream_init(dev, s->sample_rate);
}

static void usb_xblc_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBXBLCState *s = (USBXBLCState *)dev;

    if (usb_desc_handle_control(dev, p, request, value, index, length, data) >= 0) {
        DPRINTF("[XBLC] USB Control request handled by usb_desc_handle_control\n");
        return;
    }

    switch (request) {
    case VendorInterfaceOutRequest | USB_REQ_SET_FEATURE:
        if (index == XBLC_SET_SAMPLE_RATE) {
            uint8_t rate = value & 0xFF;
            assert(rate < ARRAY_SIZE(xblc_sample_rates));
            DPRINTF("[XBLC] Set Sample Rate to %04x\n", rate);
            s->sample_rate = xblc_sample_rates[rate];
            xblc_audio_stream_init(dev, s->sample_rate);
            break;
        } else if (index == XBLC_SET_AGC) {
            DPRINTF("[XBLC] Set Auto Gain Control to %d\n", value);
            s->auto_gain_control = (value) ? 1 : 0;
            break;
        }
        // Fallthrough       
    default:
        DPRINTF("[XBLC] USB stalled on request 0x%x value 0x%x\n", request, value);
        p->status = USB_RET_STALL;
        assert(false);
        return;
    }
}

static void usb_xblc_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXBLCState *s = (USBXBLCState *)dev;
    uint32_t to_process, chunk_len;

    switch (p->pid) {
    case USB_TOKEN_IN:
        // Microphone Data - Get data from fifo and copy into usb packet
        assert(p->ep->nr == XBLC_EP_IN);
        to_process = MIN(fifo8_num_used(&s->in.fifo), p->iov.size);
        chunk_len = 0;

        // fifo may not give us a contiguous packet, so may need multiple calls
        while (to_process) {
            const uint8_t *packet = fifo8_pop_bufptr(&s->in.fifo, to_process, &chunk_len);
            usb_packet_copy(p, (void *)packet, chunk_len);
            to_process -= chunk_len;
        }

        // Ensure we fill the entire packet regardless of if we have audio data so we don't
        // cause an underrun error.
        if (p->actual_length < p->iov.size)
            usb_packet_copy(p, (void *)silence, p->iov.size - p->actual_length);

        break;
    case USB_TOKEN_OUT:
        // Speaker data - get data from usb packet then push to fifo.
        assert(p->ep->nr == XBLC_EP_OUT);
        to_process = MIN(fifo8_num_free(&s->out.fifo), p->iov.size);
        usb_packet_copy(p, s->out.packet, to_process);
        fifo8_push_all(&s->out.fifo, s->out.packet, to_process);

        break;
    default:
        //Iso cannot report STALL/HALT, but we shouldn't be here anyway.
        assert(false);
        break;
    }
}

static void usb_xbox_communicator_unrealize(USBDevice *dev)
{
    USBXBLCState *s = USB_XBLC(dev);

    if (s->in.voice) {
        SDL_PauseAudioStreamDevice(s->in.voice);
    }
    if (s->out.voice) {
        SDL_PauseAudioStreamDevice(s->out.voice);
    }
    
    fifo8_destroy(&s->in.fifo);
    fifo8_destroy(&s->out.fifo);

    if (s->in.voice) {
        SDL_DestroyAudioStream(s->in.voice);
        s->in.voice = 0;
    }
    if (s->out.voice) {
        SDL_DestroyAudioStream(s->out.voice);
        s->out.voice = 0;
    }
}

static void usb_xblc_class_init(ObjectClass *klass, const void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);
    uc->handle_reset   = usb_xblc_handle_reset;
    uc->handle_control = usb_xblc_handle_control;
    uc->handle_data    = usb_xblc_handle_data;
    uc->handle_attach  = usb_desc_attach;
}

static void usb_xbox_communicator_realize(USBDevice *dev, Error **errp)
{
    USBXBLCState *s = USB_XBLC(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);

    s->in.voice = 0;
    s->out.voice = 0;

    fifo8_create(&s->in.fifo, XBLC_FIFO_SIZE);
    fifo8_create(&s->out.fifo, XBLC_FIFO_SIZE);

    s->in.volume = 1.0f;
    s->out.volume = 1.0f;
}

static const Property xblc_properties[] = {
    DEFINE_PROP_UINT8("index", USBXBLCState, device_index, 0),
};

static const VMStateDescription usb_xblc_vmstate = {
    .name = TYPE_USB_XBLC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_USB_DEVICE(dev, USBXBLCState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static void usb_xbox_communicator_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = XBLC_STR;
    uc->usb_desc       = &desc_xblc;
    uc->realize        = usb_xbox_communicator_realize;
    uc->unrealize      = usb_xbox_communicator_unrealize;
    usb_xblc_class_init(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd  = &usb_xblc_vmstate;
    device_class_set_props(dc, xblc_properties);
    dc->desc  = XBLC_STR;
}

static const TypeInfo info_xblc = {
    .name          = TYPE_USB_XBLC,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXBLCState),
    .class_init    = usb_xbox_communicator_class_init,
};

static void usb_xblc_register_types(void)
{
    type_register_static(&info_xblc);
    audio_register_model("xblc", XBLC_STR, TYPE_USB_XBLC);
}

type_init(usb_xblc_register_types)
