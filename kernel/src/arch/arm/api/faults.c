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

#include <config.h>
#include <types.h>
#include <object.h>
#include <kernel/vspace.h>
#include <api/faults.h>
#include <api/syscall.h>

bool_t
Arch_handleFaultReply(tcb_t *receiver, tcb_t *sender, word_t faultType)
{
    switch (faultType) {
    case seL4_Fault_VMFault:
        return true;

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case seL4_Fault_VGICMaintenance:
        return true;
    case seL4_Fault_VCPUFault:
        return true;
    case seL4_Fault_PSCIFault:
        return true;
#endif
    default:
        fail("Invalid fault");
    }
}

word_t
Arch_setMRs_fault(tcb_t *sender, tcb_t* receiver, word_t *receiveIPCBuffer, word_t faultType)
{
    switch (faultType) {
    case seL4_Fault_VMFault: {
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            word_t ipa, va;
            va = getRestartPC(sender);
            ipa = (addressTranslateS1CPR(va) & ~MASK(PAGE_BITS)) | (va & MASK(PAGE_BITS));
            setMR(receiver, receiveIPCBuffer, seL4_VMFault_IP, ipa);
        } else {
            setMR(receiver, receiveIPCBuffer, seL4_VMFault_IP, getRestartPC(sender));
        }
        setMR(receiver, receiveIPCBuffer, seL4_VMFault_Addr,
              seL4_Fault_VMFault_get_address(sender->tcbFault));
        setMR(receiver, receiveIPCBuffer, seL4_VMFault_PrefetchFault,
              seL4_Fault_VMFault_get_instructionFault(sender->tcbFault));
        return setMR(receiver, receiveIPCBuffer, seL4_VMFault_FSR,
                     seL4_Fault_VMFault_get_FSR(sender->tcbFault));
    }

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case seL4_Fault_VGICMaintenance:
        if (seL4_Fault_VGICMaintenance_get_idxValid(sender->tcbFault)) {
            return setMR(receiver, receiveIPCBuffer, seL4_VGICMaintenance_IDX,
                         seL4_Fault_VGICMaintenance_get_idx(sender->tcbFault));
        } else {
            return setMR(receiver, receiveIPCBuffer, seL4_VGICMaintenance_IDX, -1);
        }
    case seL4_Fault_VCPUFault:
        return setMR(receiver, receiveIPCBuffer, seL4_VCPUFault_HSR, seL4_Fault_VCPUFault_get_hsr(sender->tcbFault));

    case seL4_Fault_PSCIFault:
        return setMR(receiver, receiveIPCBuffer, seL4_PSCIFault_Function, seL4_Fault_PSCIFault_get_function(sender->tcbFault));
#endif

    default:
        fail("Invalid fault");
    }
}
