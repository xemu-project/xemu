#import <Foundation/Foundation.h>
#include "xemu-os-utils.h"

const char *xemu_get_os_info(void)
{
	return [[[NSProcessInfo processInfo] operatingSystemVersionString] UTF8String];
}

