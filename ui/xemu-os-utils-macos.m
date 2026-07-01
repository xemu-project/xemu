/*
 * OS-specific Helpers
 *
 * Copyright (C) 2020 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <dlfcn.h>
#include "xemu-os-utils.h"

const char *xemu_get_os_info(void)
{
	return [[[NSProcessInfo processInfo] operatingSystemVersionString] UTF8String];
}

#if defined(__aarch64__)
#define XEMU_MACOS_LIB_ARCH "arm64"
#else
#define XEMU_MACOS_LIB_ARCH "x86_64"
#endif

// Point the Vulkan loader at the MoltenVK ICD bundled inside the .app and load
// the bundled loader, so the Vulkan renderer works without an installed Vulkan
// SDK. Returns vkGetInstanceProcAddr from the bundled loader (for
// volkInitializeCustom) or NULL if not running from a bundle with a bundled
// loader (e.g. developer builds using an installed SDK via env vars, which fall
// back to volkInitialize). Must run before volk is initialized.
void *xemu_macos_get_bundled_vk_get_instance_proc_addr(void)
{
	@autoreleasepool {
		NSString *bundle = [[NSBundle mainBundle] bundlePath];
		if (![bundle hasSuffix:@".app"]) {
			return NULL;
		}

		NSFileManager *fm = [NSFileManager defaultManager];

		NSString *loader = [bundle stringByAppendingPathComponent:
			@"Contents/Libraries/" XEMU_MACOS_LIB_ARCH "/libvulkan.1.dylib"];
		if (![fm fileExistsAtPath:loader]) {
			return NULL;
		}

		// VK_ICD_FILENAMES is not a DYLD_* variable, so it survives the hardened
		// runtime; the bundled loader reads it to find MoltenVK. Don't override
		// an explicit developer setting.
		NSString *icd = [bundle stringByAppendingPathComponent:
			@"Contents/Resources/vulkan/icd.d/MoltenVK_icd.json"];
		if (getenv("VK_ICD_FILENAMES") == NULL &&
		    [fm fileExistsAtPath:icd]) {
			setenv("VK_ICD_FILENAMES", [icd UTF8String], 1);
			setenv("VK_DRIVER_FILES", [icd UTF8String], 1);
		}

		// dlopen the loader by absolute path (the hardened runtime ignores
		// DYLD_LIBRARY_PATH and volk searches by leaf name), then hand its
		// vkGetInstanceProcAddr to volkInitializeCustom.
		void *handle = dlopen([loader UTF8String], RTLD_NOW | RTLD_GLOBAL);
		if (!handle) {
			return NULL;
		}
		return dlsym(handle, "vkGetInstanceProcAddr");
	}
}
