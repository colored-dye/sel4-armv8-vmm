/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * Copyright 2018, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_DORNERWORKS_GPL)
 */

#pragma once

/* This file contains useful macros for assembly code. */

#ifdef __ASSEMBLER__

#define      SCTLR(reg)    p15, 0, reg, c1, c0, 0
#define      CLIDR(reg)    p15, 1, reg, c0, c0, 1
#define      TTBR0(reg)    p15, 0, reg, c2, c0, 0
#define      TTBCR(reg)    p15, 0, reg, c2, c0, 2
#define       DACR(reg)    p15, 0, reg, c3, c0, 0
#define      IIALL(reg)    p15, 0, reg, c7, c5, 0
#define     BPIALL(reg)    p15, 0, reg, c7, c5, 6
#define   DTLBIALL(reg)    p15, 0, reg, c8, c6, 0
#define    TLBIALL(reg)    p15, 0, reg, c8, c7, 0
#define  DTLBIASID(reg)    p15, 0, reg, c8, c6, 2
#define   TLBIASID(reg)    p15, 0, reg, c8, c7, 2
#define CONTEXTIDR(reg)    p15, 0, reg, c13, c0, 1

/* Processor mode encodings */
#define PMODE_USER          0x10
#define PMODE_FIQ           0x11
#define PMODE_IRQ           0x12
#define PMODE_SUPERVISOR    0x13
#define PMODE_MONITOR       0x16
#define PMODE_ABORT         0x17
#define PMODE_HYPERVISOR    0x1a
#define PMODE_UNDEFINED     0x1b
#define PMODE_SYSTEM        0x1f

/* Processor mode mask bits */
#define PMASK_MODE          (0x1f << 0)

/* Processor exception mask bits */
#define PMASK_ASYNC_ABORT   (1 << 8)
#define PMASK_IRQ           (1 << 7)
#define PMASK_FIRQ          (1 << 6)

#define CPSR_SUPERVISOR     ( PMASK_FIRQ         \
                            | PMASK_IRQ          \
                            | PMASK_ASYNC_ABORT  \
                            | PMODE_SUPERVISOR   )

#define CPSR_MONITOR        ( PMASK_FIRQ         \
                            | PMASK_IRQ          \
                            | PMASK_ASYNC_ABORT  \
                            | PMODE_MONITOR   )

#define CPSR_HYPERVISOR     ( PMASK_FIRQ         \
                            | PMASK_IRQ          \
                            | PMASK_ASYNC_ABORT  \
                            | PMODE_HYPERVISOR   )

#else /* !__ASSEMBLER__ */
#warning "Including assembly-specific header in C code"
#endif

