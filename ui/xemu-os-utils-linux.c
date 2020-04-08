#include "xemu-os-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <glib/gprintf.h>

static char *read_file_if_possible(const char *path)
{
	FILE *fd = fopen(path, "rb");
	if (fd == NULL) {
		return NULL;
	}

	fseek(fd, 0, SEEK_END);
	size_t size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	char *buf = malloc(size+1);
	int status = fread(buf, 1, size, fd);
	if (status != size) {
		free(buf);
		return NULL;
	}

	buf[size] = '\x00';
	return buf;
}

const char *xemu_get_os_info(void)
{
	static const char *os_info = NULL;
	static int attempted_init = 0;

	if (!attempted_init) {
		char *os_release = NULL;

		// Try to get the Linux distro "pretty name" from /etc/os-release
		char *os_release_file = read_file_if_possible("/etc/os-release");
		if (os_release_file != NULL) {
			char *pretty_name = strstr(os_release_file, "PRETTY_NAME=\"");
			if (pretty_name != NULL) {
				pretty_name = pretty_name + 13;
				char *pretty_name_end = strchr(pretty_name, '"');
				if (pretty_name_end != NULL) {
					size_t len = pretty_name_end-pretty_name;
					os_release = malloc(len+1);
					assert(os_release != NULL);
					memcpy(os_release, pretty_name, len);
					os_release[len] = '\x00';
				}
			}
			free(os_release_file);
		}

		os_info = g_strdup_printf("%s",
			os_release ? os_release : "Unknown Distro"
			);
		if (os_release) {
			free(os_release);
		}

		attempted_init = 1;
	}

	return os_info;
}

void xemu_open_web_browser(const char *url)
{
	char *cmd = g_strdup_printf("xdg-open %s", url);
	system(cmd);
	free(cmd);
}
