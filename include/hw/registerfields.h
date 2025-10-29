/*
 * Register Definition API: field macros
 *
 * Copyright (c) 2016 Xilinx Inc.
 * Copyright (c) 2013 Peter Crosthwaite <peter.crosthwaite@xilinx.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef REGISTERFIELDS_H
#define REGISTERFIELDS_H

#include "qemu/bitops.h"

/* Define constants for a 32 bit register */

/* This macro will define A_FOO, for the byte address of a register
 * as well as R_FOO for the uint32_t[] register number (A_FOO / 4).
 */
#define REG32(reg, addr)                                                  \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) / 4 };

#define REG8(reg, addr)                                                   \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) };

#define REG16(reg, addr)                                                  \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) / 2 };

#define REG64(reg, addr)                                                  \
    enum { A_ ## reg = (addr) };                                          \
    enum { R_ ## reg = (addr) / 8 };

/* Define SHIFT, LENGTH and MASK constants for a field within a register */

/* This macro will define R_FOO_BAR_MASK, R_FOO_BAR_SHIFT and R_FOO_BAR_LENGTH
 * constants for field BAR in register FOO.
 */
#define FIELD(reg, field, shift, length)                                  \
    enum { R_ ## reg ## _ ## field ## _SHIFT = (shift)};                  \
    enum { R_ ## reg ## _ ## field ## _LENGTH = (length)};                \
    enum { R_ ## reg ## _ ## field ## _MASK =                             \
                                        MAKE_64BIT_MASK(shift, length)};

/* Extract a field from a register */
#define FIELD_EX8(storage, reg, field)                                    \
    extract8((storage), R_ ## reg ## _ ## field ## _SHIFT,                \
              R_ ## reg ## _ ## field ## _LENGTH)
#define FIELD_EX16(storage, reg, field)                                   \
    extract16((storage), R_ ## reg ## _ ## field ## _SHIFT,               \
              R_ ## reg ## _ ## field ## _LENGTH)
#define FIELD_EX32(storage, reg, field)                                   \
    extract32((storage), R_ ## reg ## _ ## field ## _SHIFT,               \
              R_ ## reg ## _ ## field ## _LENGTH)
#define FIELD_EX64(storage, reg, field)                                   \
    extract64((storage), R_ ## reg ## _ ## field ## _SHIFT,               \
              R_ ## reg ## _ ## field ## _LENGTH)

#define FIELD_SEX8(storage, reg, field)                                   \
    sextract8((storage), R_ ## reg ## _ ## field ## _SHIFT,               \
              R_ ## reg ## _ ## field ## _LENGTH)
#define FIELD_SEX16(storage, reg, field)                                  \
    sextract16((storage), R_ ## reg ## _ ## field ## _SHIFT,              \
               R_ ## reg ## _ ## field ## _LENGTH)
#define FIELD_SEX32(storage, reg, field)                                  \
    sextract32((storage), R_ ## reg ## _ ## field ## _SHIFT,              \
               R_ ## reg ## _ ## field ## _LENGTH)
#define FIELD_SEX64(storage, reg, field)                                  \
    sextract64((storage), R_ ## reg ## _ ## field ## _SHIFT,              \
               R_ ## reg ## _ ## field ## _LENGTH)

/* Extract a field from an array of registers */
#define ARRAY_FIELD_EX32(regs, reg, field)                                \
    FIELD_EX32((regs)[R_ ## reg], reg, field)
#define ARRAY_FIELD_EX64(regs, reg, field)                                \
    FIELD_EX64((regs)[R_ ## reg], reg, field)

/* Deposit a register field.
 * Assigning values larger then the target field will result in
 * compilation warnings.
 */
#define FIELD_DP8(storage, reg, field, val) ({                            \
    struct {                                                              \
        unsigned int v:R_ ## reg ## _ ## field ## _LENGTH;                \
    } _v = { .v = val };                                                  \
    uint8_t _d;                                                           \
    _d = deposit32((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })
#define FIELD_DP16(storage, reg, field, val) ({                           \
    struct {                                                              \
        unsigned int v:R_ ## reg ## _ ## field ## _LENGTH;                \
    } _v = { .v = val };                                                  \
    uint16_t _d;                                                          \
    _d = deposit32((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })
#define FIELD_DP32(storage, reg, field, val) ({                           \
    struct {                                                              \
        unsigned int v:R_ ## reg ## _ ## field ## _LENGTH;                \
    } _v = { .v = val };                                                  \
    uint32_t _d;                                                          \
    _d = deposit32((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })
#define FIELD_DP64(storage, reg, field, val) ({                           \
    struct {                                                              \
        uint64_t v:R_ ## reg ## _ ## field ## _LENGTH;                    \
    } _v = { .v = val };                                                  \
    uint64_t _d;                                                          \
    _d = deposit64((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })

#define FIELD_SDP8(storage, reg, field, val) ({                           \
    struct {                                                              \
        signed int v:R_ ## reg ## _ ## field ## _LENGTH;                  \
    } _v = { .v = val };                                                  \
    uint8_t _d;                                                           \
    _d = deposit32((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })
#define FIELD_SDP16(storage, reg, field, val) ({                          \
    struct {                                                              \
        signed int v:R_ ## reg ## _ ## field ## _LENGTH;                  \
    } _v = { .v = val };                                                  \
    uint16_t _d;                                                          \
    _d = deposit32((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })
#define FIELD_SDP32(storage, reg, field, val) ({                          \
    struct {                                                              \
        signed int v:R_ ## reg ## _ ## field ## _LENGTH;                  \
    } _v = { .v = val };                                                  \
    uint32_t _d;                                                          \
    _d = deposit32((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })
#define FIELD_SDP64(storage, reg, field, val) ({                          \
    struct {                                                              \
        int64_t v:R_ ## reg ## _ ## field ## _LENGTH;                     \
    } _v = { .v = val };                                                  \
    uint64_t _d;                                                          \
    _d = deposit64((storage), R_ ## reg ## _ ## field ## _SHIFT,          \
                  R_ ## reg ## _ ## field ## _LENGTH, _v.v);              \
    _d; })

/* Deposit a field to array of registers.  */
#define ARRAY_FIELD_DP32(regs, reg, field, val)                           \
    (regs)[R_ ## reg] = FIELD_DP32((regs)[R_ ## reg], reg, field, val);
#define ARRAY_FIELD_DP64(regs, reg, field, val)                           \
    (regs)[R_ ## reg] = FIELD_DP64((regs)[R_ ## reg], reg, field, val);


/*
 * These macros can be used for defining and extracting fields that have the
 * same bit position across multiple registers.
 */

/* Define shared SHIFT, LENGTH, and MASK constants */
#define SHARED_FIELD(name, shift, length)   \
    enum { name ## _ ## SHIFT = (shift)};   \
    enum { name ## _ ## LENGTH = (length)}; \
    enum { name ## _ ## MASK = MAKE_64BIT_MASK(shift, length)};

/* Extract a shared field */
#define SHARED_FIELD_EX8(storage, field) \
    extract8((storage), field ## _SHIFT, field ## _LENGTH)

#define SHARED_FIELD_EX16(storage, field) \
    extract16((storage), field ## _SHIFT, field ## _LENGTH)

#define SHARED_FIELD_EX32(storage, field) \
    extract32((storage), field ## _SHIFT, field ## _LENGTH)

#define SHARED_FIELD_EX64(storage, field) \
    extract64((storage), field ## _SHIFT, field ## _LENGTH)

/* Extract a shared field from a register array */
#define SHARED_ARRAY_FIELD_EX32(regs, offset, field) \
    SHARED_FIELD_EX32((regs)[(offset)], field)
#define SHARED_ARRAY_FIELD_EX64(regs, offset, field) \
    SHARED_FIELD_EX64((regs)[(offset)], field)

/* Deposit a shared field */
#define SHARED_FIELD_DP8(storage, field, val) ({                        \
    struct {                                                            \
        unsigned int v:field ## _LENGTH;                                \
    } _v = { .v = val };                                                \
    uint8_t _d;                                                         \
    _d = deposit32((storage), field ## _SHIFT, field ## _LENGTH, _v.v); \
    _d; })

#define SHARED_FIELD_DP16(storage, field, val) ({                       \
    struct {                                                            \
        unsigned int v:field ## _LENGTH;                                \
    } _v = { .v = val };                                                \
    uint16_t _d;                                                        \
    _d = deposit32((storage), field ## _SHIFT, field ## _LENGTH, _v.v); \
    _d; })

#define SHARED_FIELD_DP32(storage, field, val) ({                       \
    struct {                                                            \
        unsigned int v:field ## _LENGTH;                                \
    } _v = { .v = val };                                                \
    uint32_t _d;                                                        \
    _d = deposit32((storage), field ## _SHIFT, field ## _LENGTH, _v.v); \
    _d; })

#define SHARED_FIELD_DP64(storage, field, val) ({                       \
    struct {                                                            \
        uint64_t v:field ## _LENGTH;                                    \
    } _v = { .v = val };                                                \
    uint64_t _d;                                                        \
    _d = deposit64((storage), field ## _SHIFT, field ## _LENGTH, _v.v); \
    _d; })

/* Deposit a shared field to a register array */
#define SHARED_ARRAY_FIELD_DP32(regs, offset, field, val) \
    (regs)[(offset)] = SHARED_FIELD_DP32((regs)[(offset)], field, val);
#define SHARED_ARRAY_FIELD_DP64(regs, offset, field, val) \
    (regs)[(offset)] = SHARED_FIELD_DP64((regs)[(offset)], field, val);

#endif
