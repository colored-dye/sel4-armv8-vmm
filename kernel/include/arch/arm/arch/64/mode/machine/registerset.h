/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * Copyright 2018, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_DORNERWORKS_GPL)
 */

#ifndef __ARCH_MACHINE_REGISTERSET_64_H
#define __ARCH_MACHINE_REGISTERSET_64_H

#include <config.h>

/* CurrentEL register */
#define PEXPL1                  (1 << 2)
#define PEXPL2                  (1 << 3)

/* PSTATE register */
#define PMODE_FIRQ              (1 << 6)
#define PMODE_IRQ               (1 << 7)
#define PMODE_SERROR            (1 << 8)
#define PMODE_DEBUG             (1 << 9)
#define PMODE_EL0t              0
#define PMODE_EL1h              5
#define PMODE_EL2h              9

/* DAIF register */
#define DAIF_FIRQ               (1 << 6)
#define DAIF_IRQ                (1 << 7)
#define DAIF_SERROR             (1 << 8)
#define DAIF_DEBUG              (1 << 9)
#define DAIFSET_MASK            0xf

/* ESR register */
#define ESR_EC_SHIFT            26
#define ESR_EC_DABT_LOW         0x24   // Data abort from a lower EL
#define ESR_EC_DABT_SAME        0x25   // Data abort from the same EL
#define ESR_EC_IABT_LOW         0x20   // Instruction abort from a lower EL
#define ESR_EC_IABT_SAME        0x21   // Instruction abort from the same EL
#define ESR_EC_SVC64            0x15   // SVC instruction execution in AArch64 state
#define ESR_EC_HVC64            0x16   // HVC instruction execution in AArch64 state
#define ESR_EC_ENFP             0x7    // Access to Advanced SIMD or floating-point registers
#define ESR_EC_SYSREG           0x18   // MSR, MRS, or System instruction execution in AArch64 state

/* ID_AA64PFR0_EL1 register */
#define ID_AA64PFR0_EL1_FP      16     // HWCap for Floating Point
#define ID_AA64PFR0_EL1_ASIMD   20     // HWCap for Advanced SIMD

/* CPACR_EL1 register */
#define CPACR_EL1_FPEN          20     // FP regiters access

/*
 * We cannot allow async aborts in the verified kernel, but they are useful
 * in identifying invalid memory access bugs so we enable them in debug mode.
 */
#ifdef CONFIG_DEBUG_BUILD
#define PSTATE_EXTRA_FLAGS  0
#else
#define PSTATE_EXTRA_FLAGS  PMODE_SERROR
#endif

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define PSTATE_USER         (PMODE_FIRQ | PMODE_IRQ  | PMODE_DEBUG | PMODE_EL1h | PSTATE_EXTRA_FLAGS)
#define PSTATE_IDLETHREAD   (PMODE_FIRQ | PMODE_EL2h | PSTATE_EXTRA_FLAGS)
#else
#define PSTATE_USER         (PMODE_FIRQ | PMODE_EL0t | PSTATE_EXTRA_FLAGS)
#define PSTATE_IDLETHREAD   (PMODE_FIRQ | PMODE_EL1h | PSTATE_EXTRA_FLAGS)
#endif

/* Offsets within the user context, these need to match the order in
 * register_t below */
#define PT_LR                       (30 * 8)
#define PT_SP_EL0                   (31 * 8)
#define PT_ELR_EL1                  (32 * 8)
#define PT_SPSR_EL1                 (33 * 8)
#define PT_TPIDRURW                 (35 * 8)
#define PT_FaultInstruction         (34 * 8)

#ifndef __ASSEMBLER__ /* C only definitions */

#include <config.h>
#include <stdint.h>
#include <assert.h>
#include <util.h>
#include <arch/types.h>
#include <plat/api/constants.h>

/* These are the indices of the registers in the saved thread context.
 * The values are determined by the order in which they're saved in the trap handler. */
enum _register {
    X0                          = 0,    /* 0x00 */
    capRegister                 = 0,
    badgeRegister               = 0,

    X1                          = 1,    /* 0x08 */
    msgInfoRegister             = 1,

    X2                          = 2,    /* 0x10 */
    X3                          = 3,    /* 0x18 */
    X4                          = 4,    /* 0x20 */
    X5                          = 5,    /* 0x28 */
    X6                          = 6,    /* 0x30 */
    X7                          = 7,    /* 0x38 */
    X8                          = 8,    /* 0x40 */
    X9                          = 9,    /* 0x48 */
    X10                         = 10,   /* 0x50 */
    X11                         = 11,   /* 0x58 */
    X12                         = 12,   /* 0x60 */
    X13                         = 13,   /* 0x68 */
    X14                         = 14,   /* 0x70 */
    X15                         = 15,   /* 0x78 */
    X16                         = 16,   /* 0x80 */
    X17                         = 17,   /* 0x88 */
    X18                         = 18,   /* 0x90 */
    X19                         = 19,   /* 0x98 */
    X20                         = 20,   /* 0xa0 */
    X21                         = 21,   /* 0xa8 */
    X22                         = 22,   /* 0xb0 */
    X23                         = 23,   /* 0xb8 */
    X24                         = 24,   /* 0xc0 */
    X25                         = 25,   /* 0xc8 */
    X26                         = 26,   /* 0xd0 */
    X27                         = 27,   /* 0xd8 */
    X28                         = 28,   /* 0xe0 */
    X29                         = 29,   /* 0xe8 */

    X30                         = 30,   /* 0xf0 */
    LR                          = 30,

    /* End of GP registers, the following are additional kernel-saved state. */

    /* Note: In Hyp Mode, EL2 is used for the Kernel and EL1 for the Applications.
     *       This means that sp_el1, elr_el2, and spsr_el2 actually get stored in
     *       these registers, but we keep the slots open for consistency around
     *       the code.
     */
    SP_EL0                      = 31,   /* 0xf8 */
    ELR_EL1                     = 32,   /* 0x100 */
    LR_svc                      = 32,
    SPSR_EL1                    = 33,   /* 0x108 */

    FaultInstruction            = 34,   /* 0x110 */
    /* user readable/writable thread ID register.
     * name comes from the ARM manual */
    TPIDRURW                    = 35,
    n_contextRegisters          = 36,
};

compile_assert(sp_offset_correct, SP_EL0 * sizeof(word_t) == PT_SP_EL0)
compile_assert(lr_svc_offset_correct, ELR_EL1 * sizeof(word_t) == PT_ELR_EL1)
compile_assert(faultinstruction_offset_correct, FaultInstruction * sizeof(word_t) == PT_FaultInstruction)

typedef word_t register_t;

enum messageSizes {
    n_msgRegisters = seL4_FastMessageRegisters,
    n_frameRegisters = 17,
    n_gpRegisters = 17,
    n_exceptionMessage = 3,
    n_syscallMessage = 12,
};

#define EXCEPTION_MESSAGE \
 {\
    [seL4_UserException_FaultIP] = FaultInstruction,\
    [seL4_UserException_SP] = SP_EL0,\
    [seL4_UserException_SPSR] = SPSR_EL1\
 }

#define SYSCALL_MESSAGE \
{\
    [seL4_UnknownSyscall_X0] = X0,\
    [seL4_UnknownSyscall_X1] = X1,\
    [seL4_UnknownSyscall_X2] = X2,\
    [seL4_UnknownSyscall_X3] = X3,\
    [seL4_UnknownSyscall_X4] = X4,\
    [seL4_UnknownSyscall_X5] = X5,\
    [seL4_UnknownSyscall_X6] = X6,\
    [seL4_UnknownSyscall_X7] = X7,\
    [seL4_UnknownSyscall_FaultIP] = FaultInstruction,\
    [seL4_UnknownSyscall_SP] = SP_EL0,\
    [seL4_UnknownSyscall_LR] = ELR_EL1,\
    [seL4_UnknownSyscall_SPSR] = SPSR_EL1\
}

extern const register_t msgRegisters[];
extern const register_t frameRegisters[];
extern const register_t gpRegisters[];

#ifdef CONFIG_HAVE_FPU
typedef struct user_fpu_state {
    __uint128_t vregs[32];
    uint32_t fpsr;
    uint32_t fpcr;
} user_fpu_state_t;
#endif /* CONFIG_HAVE_FPU */

/* ARM user-code context: size = 72 bytes
 * Or with hardware debug support built in:
 *      72 + sizeof(word_t) * (NUM_BPS + NUM_WPS) * 2
 *
 * The "word_t registers" member of this struct must come first, because in
 * head.S, we assume that an "ldr %0, =ksCurThread" will point to the beginning
 * of the current thread's registers. The assert below should help.
 */
struct user_context {
    word_t registers[n_contextRegisters];
#ifdef CONFIG_HAVE_FPU
    user_fpu_state_t fpuState;
#endif /* CONFIG_HAVE_FPU */
};
typedef struct user_context user_context_t;

unverified_compile_assert(registers_are_first_member_of_user_context,
                          OFFSETOF(user_context_t, registers) == 0)


static inline void Arch_initContext(user_context_t* context)
{
    context->registers[SPSR_EL1] = PSTATE_USER;
}

#endif /* !__ASSEMBLER__ */

#endif /* __ARCH_MACHINE_REGISTERSET_64_H */
