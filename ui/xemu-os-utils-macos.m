#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "xemu-os-utils.h"

const char *xemu_get_os_info(void)
{
	return [[[NSProcessInfo processInfo] operatingSystemVersionString] UTF8String];
}

void xemu_open_web_browser(const char *url)
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}
