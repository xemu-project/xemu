#ifndef XEMU_OS_UTILS_H
#define XEMU_OS_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

const char *xemu_get_os_info(void);
void xemu_open_web_browser(const char *url);
	
#ifdef __cplusplus
}
#endif

#endif
