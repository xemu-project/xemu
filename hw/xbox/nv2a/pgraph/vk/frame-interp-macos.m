/*
 * Frame Interpolation for macOS via VideoToolbox VTFrameProcessor
 *
 * Uses VTFrameRateConversion to generate synthetic intermediate frames
 * between real 30fps game frames, producing smooth 60fps output.
 *
 * All heavy work (vImageScale + VTFrameProcessor) runs asynchronously on a
 * GCD serial queue. The render loop never blocks on frame interpolation.
 * Double-buffered output ensures tear-free reads.
 *
 * Copyright (c) 2025 xemu contributors
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

#import <Foundation/Foundation.h>
#import <VideoToolbox/VTFrameProcessor.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <IOSurface/IOSurface.h>
#import <Accelerate/Accelerate.h>
#include <stdatomic.h>

#include "frame-interp-macos.h"

// Ring buffer of 2 frames (prev + current)
#define RING_SIZE 2

// Cap interpolation resolution for performance. VTFrameProcessor has ~15ms
// of fixed neural engine overhead regardless of resolution.
#define INTERP_MAX_DIM 1280

static struct {
    bool initialized;
    bool available;
    int src_width;   // source IOSurface dimensions
    int src_height;
    int width;       // interpolation dimensions (may be smaller than source)
    int height;
    bool needs_scale;

    id processor; // VTFrameProcessor * (macOS 15.4+)
    id config;    // VTFrameRateConversionConfiguration * (macOS 15.4+)

    CVPixelBufferRef ring[RING_SIZE];
    atomic_int ring_head; // points to current (most recent) frame
    atomic_int frame_count;

    // Double-buffered async output
    CVPixelBufferRef interp_output[2];
    IOSurfaceRef interp_iosurface[2];
    atomic_int ready_idx;      // index of buffer ready for display (-1 = none)
    atomic_int ready_fc;       // frame_count when ready result was computed
    atomic_bool processing;    // true while async VT is running
    atomic_bool pending;       // true if new frames arrived during processing
    dispatch_queue_t queue;    // serial queue for async processing
} g_interp;

static void do_vt_processing(void);

static CVPixelBufferRef create_iosurface_pixel_buffer(int width, int height)
{
    NSDictionary *attrs = @{
        (NSString *)kCVPixelBufferWidthKey: @(width),
        (NSString *)kCVPixelBufferHeightKey: @(height),
        (NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{},
    };

    CVPixelBufferRef buffer = NULL;
    CVReturn status = CVPixelBufferCreate(
        kCFAllocatorDefault, width, height,
        kCVPixelFormatType_32BGRA,
        (__bridge CFDictionaryRef)attrs,
        &buffer);

    if (status != kCVReturnSuccess) {
        fprintf(stderr, "[frame-interp] Failed to create pixel buffer: %d\n",
                status);
        return NULL;
    }
    return buffer;
}

bool frame_interp_is_available(void)
{
    if (@available(macOS 15.4, *)) {
        return [VTFrameRateConversionConfiguration processorSupported];
    }
    return false;
}

bool frame_interp_init(int width, int height)
{
    if (g_interp.initialized) {
        frame_interp_finalize();
    }

    memset(&g_interp, 0, sizeof(g_interp));
    atomic_store(&g_interp.ready_idx, -1);
    atomic_store(&g_interp.ready_fc, 0);
    atomic_store(&g_interp.processing, false);
    atomic_store(&g_interp.pending, false);

    if (!frame_interp_is_available()) {
        fprintf(stderr, "[frame-interp] VTFrameRateConversion not available\n");
        return false;
    }

    if (@available(macOS 15.4, *)) {
        g_interp.src_width = width;
        g_interp.src_height = height;

        // Cap interpolation resolution for performance
        int interp_w = width, interp_h = height;
        int max_dim = MAX(interp_w, interp_h);
        if (max_dim > INTERP_MAX_DIM) {
            float scale = (float)INTERP_MAX_DIM / max_dim;
            interp_w = (int)(interp_w * scale) & ~1; // keep even
            interp_h = (int)(interp_h * scale) & ~1;
        }
        g_interp.width = interp_w;
        g_interp.height = interp_h;
        g_interp.needs_scale = (interp_w != width || interp_h != height);

        // Create frame rate conversion configuration
        VTFrameRateConversionConfiguration *config =
            [[VTFrameRateConversionConfiguration alloc]
                initWithFrameWidth:interp_w
                       frameHeight:interp_h
                usePrecomputedFlow:NO
             qualityPrioritization:VTFrameRateConversionConfigurationQualityPrioritizationNormal
                          revision:VTFrameRateConversionConfigurationRevision1];
        g_interp.config = config;
        if (!config) {
            fprintf(stderr, "[frame-interp] Failed to create FRC config\n");
            return false;
        }

        // Create processor and start session
        VTFrameProcessor *processor = [[VTFrameProcessor alloc] init];
        g_interp.processor = processor;
        NSError *error = nil;
        BOOL ok = [processor
            startSessionWithConfiguration:config
                                    error:&error];
        if (!ok) {
            fprintf(stderr, "[frame-interp] Failed to start session: %s\n",
                    error.localizedDescription.UTF8String);
            g_interp.processor = nil;
            g_interp.config = nil;
            return false;
        }

        // Allocate ring buffer CVPixelBuffers at interpolation resolution
        for (int i = 0; i < RING_SIZE; i++) {
            g_interp.ring[i] =
                create_iosurface_pixel_buffer(interp_w, interp_h);
            if (!g_interp.ring[i]) {
                frame_interp_finalize();
                return false;
            }
        }

        // Allocate double-buffered output
        for (int i = 0; i < 2; i++) {
            g_interp.interp_output[i] =
                create_iosurface_pixel_buffer(interp_w, interp_h);
            if (!g_interp.interp_output[i]) {
                frame_interp_finalize();
                return false;
            }
            g_interp.interp_iosurface[i] =
                CVPixelBufferGetIOSurface(g_interp.interp_output[i]);
        }

        // Create serial dispatch queue for async processing
        g_interp.queue = dispatch_queue_create(
            "org.xemu.frame-interp", DISPATCH_QUEUE_SERIAL);

        g_interp.initialized = true;
        g_interp.available = true;
        atomic_store(&g_interp.frame_count, 0);
        atomic_store(&g_interp.ring_head, 0);

        return true;
    }

    return false;
}

void frame_interp_finalize(void)
{
    // Wait for any pending async work to complete
    if (g_interp.queue) {
        dispatch_sync(g_interp.queue, ^{});
    }

    if (@available(macOS 15.4, *)) {
        if (g_interp.processor) {
            [(VTFrameProcessor *)g_interp.processor endSession];
            g_interp.processor = nil;
        }
        g_interp.config = nil;
    }

    for (int i = 0; i < 2; i++) {
        if (g_interp.interp_output[i]) {
            CVPixelBufferRelease(g_interp.interp_output[i]);
            g_interp.interp_output[i] = NULL;
        }
        g_interp.interp_iosurface[i] = NULL;
    }

    for (int i = 0; i < RING_SIZE; i++) {
        if (g_interp.ring[i]) {
            CVPixelBufferRelease(g_interp.ring[i]);
            g_interp.ring[i] = NULL;
        }
    }

    if (g_interp.queue) {
        g_interp.queue = nil;
    }

    atomic_store(&g_interp.ready_idx, -1);
    atomic_store(&g_interp.ready_fc, 0);
    atomic_store(&g_interp.processing, false);
    atomic_store(&g_interp.pending, false);
    g_interp.initialized = false;
    g_interp.available = false;
}

// Run VT processing on the current ring buffer contents.
// Must be called from the dispatch queue.
static void do_vt_processing(void)
{
    @autoreleasepool {
    if (@available(macOS 15.4, *)) {
        int head = atomic_load(&g_interp.ring_head);
        int prev_idx = (head + RING_SIZE - 1) % RING_SIZE;
        int curr_idx = head;
        int fc = atomic_load(&g_interp.frame_count);

        // Write to the buffer NOT currently being displayed
        int ready = atomic_load(&g_interp.ready_idx);
        int write_idx = (ready == 0) ? 1 : 0;

        CMTime prev_time = CMTimeMake(fc - 1, 30);
        CMTime curr_time = CMTimeMake(fc, 30);

        VTFrameProcessorFrame *src_frame =
            [[VTFrameProcessorFrame alloc]
                initWithBuffer:g_interp.ring[prev_idx]
                presentationTimeStamp:prev_time];
        VTFrameProcessorFrame *next_frame =
            [[VTFrameProcessorFrame alloc]
                initWithBuffer:g_interp.ring[curr_idx]
                presentationTimeStamp:curr_time];

        if (!src_frame || !next_frame) {
            return;
        }

        VTFrameProcessorFrame *dest_frame =
            [[VTFrameProcessorFrame alloc]
                initWithBuffer:g_interp.interp_output[write_idx]
                presentationTimeStamp:CMTimeMake(fc * 2 - 1, 60)];

        if (!dest_frame) {
            return;
        }

        VTFrameRateConversionParameters *params =
            [[VTFrameRateConversionParameters alloc]
                initWithSourceFrame:src_frame
                          nextFrame:next_frame
                        opticalFlow:nil
                 interpolationPhase:@[@0.5]
                     submissionMode:
                      VTFrameRateConversionParametersSubmissionModeSequential
                  destinationFrames:@[dest_frame]];

        if (!params) {
            return;
        }

        NSError *error = nil;
        VTFrameProcessor *processor =
            (VTFrameProcessor *)g_interp.processor;
        BOOL ok = [processor processWithParameters:params
                                             error:&error];
        if (ok) {
            // Atomically publish the new result with its frame count
            atomic_store(&g_interp.ready_fc, fc);
            atomic_store(&g_interp.ready_idx, write_idx);
        }
    }
    } // @autoreleasepool
}

// Copy IOSurface data into the ring buffer slot (with optional downscale).
// Must be called from the dispatch queue.
static void do_push_copy(IOSurfaceRef surface, int head)
{
    CVPixelBufferRef dst = g_interp.ring[head];

    IOSurfaceLock(surface, kIOSurfaceLockReadOnly, NULL);
    CVPixelBufferLockBaseAddress(dst, 0);

    if (g_interp.needs_scale) {
        vImage_Buffer src_buf = {
            .data = IOSurfaceGetBaseAddress(surface),
            .width = (vImagePixelCount)IOSurfaceGetWidth(surface),
            .height = (vImagePixelCount)IOSurfaceGetHeight(surface),
            .rowBytes = IOSurfaceGetBytesPerRow(surface),
        };
        vImage_Buffer dst_buf = {
            .data = CVPixelBufferGetBaseAddress(dst),
            .width = (vImagePixelCount)g_interp.width,
            .height = (vImagePixelCount)g_interp.height,
            .rowBytes = CVPixelBufferGetBytesPerRow(dst),
        };
        vImageScale_ARGB8888(&src_buf, &dst_buf, NULL, kvImageNoFlags);
    } else {
        void *src_ptr = IOSurfaceGetBaseAddress(surface);
        void *dst_ptr = CVPixelBufferGetBaseAddress(dst);
        size_t src_stride = IOSurfaceGetBytesPerRow(surface);
        size_t dst_stride = CVPixelBufferGetBytesPerRow(dst);
        size_t copy_height = MIN((size_t)g_interp.height,
                                 IOSurfaceGetHeight(surface));
        size_t copy_width_bytes = MIN(src_stride, dst_stride);

        for (size_t y = 0; y < copy_height; y++) {
            memcpy((uint8_t *)dst_ptr + y * dst_stride,
                   (uint8_t *)src_ptr + y * src_stride,
                   copy_width_bytes);
        }
    }

    CVPixelBufferUnlockBaseAddress(dst, 0);
    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, NULL);
}

void frame_interp_push_frame(IOSurfaceRef surface)
{
    if (!g_interp.initialized || !surface) {
        return;
    }

    // Advance ring head and frame count atomically (main thread only)
    int head = (atomic_load(&g_interp.ring_head) + 1) % RING_SIZE;
    atomic_store(&g_interp.ring_head, head);
    int fc = atomic_fetch_add(&g_interp.frame_count, 1) + 1;

    // Retain IOSurface for the async block
    CFRetain(surface);

    // If VT is currently processing, mark pending so it re-dispatches
    // when done with the latest frames
    if (atomic_load(&g_interp.processing)) {
        atomic_store(&g_interp.pending, true);
    }

    // Dispatch all heavy work (copy + VT) to background queue.
    // Capture head and fc by value so the async block uses a snapshot.
    dispatch_async(g_interp.queue, ^{
        if (!g_interp.initialized) {
            CFRelease(surface);
            return;
        }

        // Copy/scale IOSurface → ring buffer
        do_push_copy(surface, head);
        CFRelease(surface);

        // Only start VT if we have 2+ frames and nothing is already running
        if (fc < 2) {
            return;
        }

        // Mark as processing (if not already)
        bool expected = false;
        if (!atomic_compare_exchange_strong(&g_interp.processing,
                                            &expected, true)) {
            // Another VT job is still running — pending flag is already set
            return;
        }

        do_vt_processing();

        // Check if new frames arrived while we were processing
        while (atomic_exchange(&g_interp.pending, false)) {
            do_vt_processing();
        }

        atomic_store(&g_interp.processing, false);
    });
}

IOSurfaceRef frame_interp_get_interpolated(void)
{
    if (!g_interp.initialized || atomic_load(&g_interp.frame_count) < 2) {
        return NULL;
    }

    // Non-blocking: return the most recently completed result.
    // Skip if the result is too stale (>2 game frames behind).
    int idx = atomic_load(&g_interp.ready_idx);
    if (idx >= 0 && idx < 2) {
        int result_fc = atomic_load(&g_interp.ready_fc);
        int current_fc = atomic_load(&g_interp.frame_count);
        if (current_fc - result_fc <= 2) {
            return g_interp.interp_iosurface[idx];
        }
    }

    return NULL;
}
