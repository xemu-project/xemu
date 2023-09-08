#ifndef S390X_TARGET_SYSCALL_H
#define S390X_TARGET_SYSCALL_H

/* this typedef defines how a Program Status Word looks like */
typedef struct {
    abi_ulong mask;
    abi_ulong addr;
} __attribute__ ((aligned(8))) target_psw_t;

/*
 * The pt_regs struct defines the way the registers are stored on
 * the stack during a system call.
 */

#define TARGET_NUM_GPRS        16

struct target_pt_regs {
    abi_ulong args[1];
    target_psw_t psw;
    abi_ulong gprs[TARGET_NUM_GPRS];
    abi_ulong orig_gpr2;
    unsigned short ilen;
    unsigned short trap;
};

#define UNAME_MACHINE "s390x"
#define UNAME_MINIMUM_RELEASE "2.6.32"

#define TARGET_CLONE_BACKWARDS2
#define TARGET_MCL_CURRENT 1
#define TARGET_MCL_FUTURE  2
#define TARGET_MCL_ONFAULT 4

#endif /* S390X_TARGET_SYSCALL_H */
