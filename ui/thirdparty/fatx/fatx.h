#ifndef FATX_H
#define FATX_H

#include "qemu/osdep.h"

#ifdef __cplusplus
extern "C" {
#endif

bool create_fatx_image(const char *filename, unsigned int size);

#ifdef __cplusplus
}
#endif

#endif
