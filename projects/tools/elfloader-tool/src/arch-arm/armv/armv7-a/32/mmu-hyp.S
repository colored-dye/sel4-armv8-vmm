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

#include <autoconf.h>

#include <assembler.h>
#include <armv/assembler.h>

.text

.extern _lpae_boot_pgd
.extern flush_dcache
.extern invalidate_dcache
.extern invalidate_icache

BEGIN_FUNC(leave_hyp)
    .arch_extension virt
    ldr r0, =CPSR_SUPERVISOR
    msr spsr_hyp, r0
    msr sp_svc, sp
    msr elr_hyp, lr
    eret
END_FUNC(leave_hyp)

BEGIN_FUNC(leave_sys)
    .arch_extension sec
    msr lr_mon, lr
    msr sp_mon, sp
    cps #PMODE_MONITOR
    isb
    bx  lr
END_FUNC(leave_sys)

BEGIN_FUNC(enable_hyp)
    .arch_extension virt
    /* Enable non-secure hypervisor (SCR) */
    mrc  p15, 0, r1, c1, c1, 0
    orr  r1, r1, #0x31
    orr  r1, r1, #0x100
    mcr  p15, 0, r1, c1, c1, 0
    isb
    /* Clear VOFF */
    mov  r1, #0
    mcrr p15, 4, r1, r1, c14
    /* Reset HSCTLR */
    ldr  r1, =0x30C50830
    mcr  p15, 4, r1, c1, c0, 0
    /* Enable NS access to FP/SIMD (NSACR, HCPTR, and CPACR) */
    ldr  r1, =0x00000C00
    mcr  p15, 0, r1, c1, c1, 2
    ldr  r1, =0x000033FF
    mcr  p15, 4, r1, c1, c1, 2
    ldr  r1, =0x00F00000
    mcr  p15, 0, r1, c1, c0, 2
    bx   lr
END_FUNC(enable_hyp)

BEGIN_FUNC(leave_el3)
    .arch_extension sec
    mrs r3, CPSR
    and r3, r3, #PMASK_MODE
    cmp r3, #PMODE_MONITOR
    beq in_mon
    mov r0, lr
    bl  leave_sys
in_mon:
    bl  enable_hyp
    mov lr, r0
    ldr r0, =CPSR_HYPERVISOR
    msr spsr_mon, r0
    msr sp_hyp, sp
    isb
    eret
END_FUNC(leave_el3)

/*
 * Enable the ARM MMU.
 *
 * It is expected that the code of this function will be mapped 1:1
 * virtual/physical in the pagetable we activate.
 */
BEGIN_FUNC(arm_enable_hyp_mmu)
    stmfd   sp!, {lr}

    /* Clean D-Cache if enabled */
    mrc     HSCTLR(r1)
    and     r1, r1, #(1 << 2)
    cmp     r1, #0
    bleq    flush_dcache

    /* Ensure I-cache, D-cache and mmu are disabled. */
    mrc     HSCTLR(r1)
    bic     r1, r1, #(1 << 12) /* Disable I-cache */
    bic     r1, r1, #(1 << 2)  /* Disable D-Cache */
    bic     r1, r1, #(1 << 0)  /* Disable MMU */
    mcr     HSCTLR(r1)

    /* invalidate caches. */
    bl      invalidate_dcache
    bl      invalidate_icache

    /* Setup MAIR - Strongly ordered non-cachable for all index */
    mov     r1, #0
    mcr     HMAIR0(r1)
    mcr     HMAIR1(r1)

    /* Set up the page table */
    ldr     r0, =_lpae_boot_pgd
    mov     r1, #0
    mov     r2, #(1<<31)
    mcrr    HTTBR(r1,r0)
    mcr     HTCR(r2)

    /* Invalidate TLB */
    mcr     DTLBIALL(r1)

    /* Enable MMU, D-cache, and I-cache. */
    mrc     HSCTLR(r0)
    orr     r0, r0, #(1 << 2)  // enable dcache
    orr     r0, r0, #(1 << 12) // enable icache
    orr     r0, r0, #(1 << 0)  // MMU enable
    mcr     HSCTLR(r0)

#if CONFIG_MAX_NUM_NODES > 1
    /* Enable SMP */
    mrc     p15, 0, r0, c1, c0, 1
    orr     r0, r0, #(1 << 6) // enable SMP bit
    mcr     p15, 0, r0, c1, c0, 1
#endif /* CONFIG_MAX_NUM_NODES > 1 */

    ldmfd   sp!, {pc}
END_FUNC(arm_enable_hyp_mmu)
