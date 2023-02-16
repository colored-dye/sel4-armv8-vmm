/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#ifndef __PLAT_MACHINE_HARDWARE_H
#define __PLAT_MACHINE_HARDWARE_H

#include <config.h>
#include <basic_types.h>
#include <linker.h>
#include <plat/machine.h>
#include <plat/machine/devices.h>

#define physBase            0x80000000

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
/* The HYP mode kernel uses TTBR0_EL2 which covers the range of
 * 0x0 - 0x0000ffffffffffff.
 */
#define kernelBase          0x0000ff8080000000llu

/* The userspace occupies the range 0x0 to 0xfffffffffff.
 * The stage-1 translation is disabled, and the stage-2
 * translation input addree size is constrained by the
 * ID_AA64MMFR0_EL1.PARange which is 44 bits on TX1.
 * Anything address above the range above triggers an
 * address size fault.
 */
#define USER_TOP            0x00000fffffffffffllu
#else
#define kernelBase          0xffffff8000000000llu

/* Maximum virtual address accessible from userspace */
#define USER_TOP            0x00007fffffffffff

#endif

static const kernel_frame_t BOOT_RODATA kernel_devices[] = {
    {
        /* map kernel device: GIC */
        GIC_CONTROLLER_PADDR,
        GIC_CONTROLLER_PPTR,
        1,
        true  /* armExecuteNever */
    },
    {
        GIC_DISTRIBUTOR_PADDR,
        GIC_DISTRIBUTOR_PPTR,
        1,
        true  /* armExecuteNever */
#ifdef CONFIG_PRINTING
    },
    {
        /* UART */
        UARTA_PADDR,
        UARTA_PPTR,
        1,
        true
#endif /* CONFIG_PRINTING */
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    },
    {
        GICH_PADDR,
        GICH_PPTR,
        true
#endif
    }
};

/* Available physical memory regions on platform (RAM minus kernel image). */
/* NOTE: Regions are not allowed to be adjacent! */

/* The TegraBoot carveout memory regions starting from 0xff03f000, so we
 * skip the 0xff000000 to 0xffffffff region. The carveout regions may change
 * if the TegraBoot version changes, so keep an eye on the booting process
 * if something strange happens.
 */
static const p_region_t BOOT_RODATA avail_p_regs[] = {
    { .start = 0x80000000, .end = 0xff000000 },
    { .start = 0x100000000, .end = 0x180000000 }
};

static const p_region_t BOOT_RODATA dev_p_regs[] = {
    { UARTA_SYNC_PADDR,     UARTA_SYNC_PADDR + (BIT(seL4_PageBits) * 3 ) },    /* 12 KB, multiple */
    { TMR_PADDR,            TMR_PADDR + BIT(seL4_PageBits) }                   /* 4 Kb            */
};

/* Handle a platform-reserved IRQ. */
static inline void
handleReservedIRQ(irq_t irq)
{

}

#endif /* __PLAT_MACHINE_HARDWARE_H */
