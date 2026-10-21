#ifndef QEMU_FAST_HASH_H
#define QEMU_FAST_HASH_H

#include <stdint.h>
#include <string.h>

uint64_t fast_hash(const uint8_t *data, size_t len);

#endif /* QEMU_FAST_HASH_H */
