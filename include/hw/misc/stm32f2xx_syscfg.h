/*
 * STM32F2XX SYSCFG
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HW_STM32F2XX_SYSCFG_H
#define HW_STM32F2XX_SYSCFG_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define SYSCFG_MEMRMP  0x00
#define SYSCFG_PMC     0x04
#define SYSCFG_EXTICR1 0x08
#define SYSCFG_EXTICR2 0x0C
#define SYSCFG_EXTICR3 0x10
#define SYSCFG_EXTICR4 0x14
#define SYSCFG_CMPCR   0x20

#define TYPE_STM32F2XX_SYSCFG "stm32f2xx-syscfg"
OBJECT_DECLARE_SIMPLE_TYPE(STM32F2XXSyscfgState, STM32F2XX_SYSCFG)

struct STM32F2XXSyscfgState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t syscfg_memrmp;
    uint32_t syscfg_pmc;
    uint32_t syscfg_exticr1;
    uint32_t syscfg_exticr2;
    uint32_t syscfg_exticr3;
    uint32_t syscfg_exticr4;
    uint32_t syscfg_cmpcr;
};

#endif /* HW_STM32F2XX_SYSCFG_H */
