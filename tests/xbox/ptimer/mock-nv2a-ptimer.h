/*
 * Header for Xbox NV2A PTIMER mocks
 */

#ifndef MOCK_NV2A_PTIMER_H
#define MOCK_NV2A_PTIMER_H

#include "qemu/osdep.h"
#include "hw/xbox/nv2a/nv2a_int.h"

extern int64_t mock_virtual_time_ns;
extern int64_t last_timer_mod_expire;
extern bool mock_timer_active;
extern int irq_update_count;

void mock_ptimer_reset(void);

#endif /* MOCK_NV2A_PTIMER_H */
