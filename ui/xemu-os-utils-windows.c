#include "xemu-os-utils.h"
#include <windows.h>

const char *xemu_get_os_info(void)
{
	return "Windows";
}

void xemu_open_web_browser(const char *url)
{
	ShellExecute(0, "open", url, 0, 0 , SW_SHOW);
}
