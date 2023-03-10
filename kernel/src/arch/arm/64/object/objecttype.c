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

#include <config.h>
#include <types.h>
#include <api/failures.h>
#include <kernel/vspace.h>
#include <object/structures.h>
#include <arch/machine.h>
#include <arch/model/statedata.h>
#include <arch/object/objecttype.h>
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#include <arch/object/vcpu.h>
#endif

bool_t
Arch_isFrameType(word_t type)
{
    switch (type) {
    case seL4_ARM_SmallPageObject:
        return true;
    case seL4_ARM_LargePageObject:
        return true;
    case seL4_ARM_HugePageObject:
        return true;
    default:
        return false;
    }
}

deriveCap_ret_t
Arch_deriveCap(cte_t *slot, cap_t cap)
{
    deriveCap_ret_t ret;

    switch (cap_get_capType(cap)) {
    case cap_page_global_directory_cap:
        if (cap_page_global_directory_cap_get_capPGDIsMapped(cap)) {
            ret.cap = cap;
            ret.status = EXCEPTION_NONE;
        } else {
            userError("Deriving a PDG cap without an assigned ASID");
            current_syscall_error.type = seL4_IllegalOperation;
            ret.cap = cap_null_cap_new();
            ret.status = EXCEPTION_SYSCALL_ERROR;
        }
        return ret;

    case cap_page_upper_directory_cap:
        if (cap_page_upper_directory_cap_get_capPUDIsMapped(cap)) {
            ret.cap = cap;
            ret.status = EXCEPTION_NONE;
        } else {
            userError("Deriving a PUD cap without an assigned ASID");
            current_syscall_error.type = seL4_IllegalOperation;
            ret.cap = cap_null_cap_new();
            ret.status = EXCEPTION_SYSCALL_ERROR;
        }
        return ret;

    case cap_page_directory_cap:
        if (cap_page_directory_cap_get_capPDIsMapped(cap)) {
            ret.cap = cap;
            ret.status = EXCEPTION_NONE;
        } else {
            userError("Deriving a PD cap without an assigned ASID");
            current_syscall_error.type = seL4_IllegalOperation;
            ret.cap = cap_null_cap_new();
            ret.status = EXCEPTION_SYSCALL_ERROR;
        }
        return ret;

    case cap_page_table_cap:
        if (cap_page_table_cap_get_capPTIsMapped(cap)) {
            ret.cap = cap;
            ret.status = EXCEPTION_NONE;
        } else {
            userError("Deriving a PT cap without an assigned ASID");
            current_syscall_error.type = seL4_IllegalOperation;
            ret.cap = cap_null_cap_new();
            ret.status = EXCEPTION_SYSCALL_ERROR;
        }
        return ret;

    case cap_frame_cap:
        ret.cap = cap_frame_cap_set_capFMappedASID(cap, asidInvalid);
        ret.status = EXCEPTION_NONE;
        return ret;

    case cap_asid_control_cap:
    case cap_asid_pool_cap:
        ret.cap = cap;
        ret.status = EXCEPTION_NONE;
        return ret;

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case cap_vcpu_cap:
        ret.cap = cap;
        ret.status = EXCEPTION_NONE;
        return ret;
#endif

#ifdef CONFIG_ARM_SMMU
    case cap_io_space_cap:
        ret.cap = cap;
        ret.status = EXCEPTION_NONE;
        return ret;

    case cap_io_page_table_cap:
        if (cap_io_page_table_cap_get_capIOPTIsMapped(cap)) {
            ret.cap = cap;
            ret.status = EXCEPTION_NONE;
        } else {
            userError("Deriving a IOPT cap without an assigned IOASID");
            current_syscall_error.type = seL4_IllegalOperation;
            ret.cap = cap_null_cap_new();
            ret.status = EXCEPTION_SYSCALL_ERROR;
        }
        return ret;
#endif

    default:
        /* This assert has no equivalent in haskell,
         * as the options are restricted by type */
        fail("Invalid arch cap");
    }
}

cap_t CONST
Arch_updateCapData(bool_t preserve, word_t data, cap_t cap)
{
   switch (cap_get_capType(cap)) {
#ifdef CONFIG_ARM_SMMU_V2
    case cap_io_space_cap: {
        io_space_capdata_t w = { { data } };
        uint32_t streamID = io_space_capdata_get_streamID(w);
        if (!preserve && cap_io_space_cap_get_capStreamID(cap) == 0 &&
                streamID != 0) {
            return cap_io_space_cap_new(streamID);
        } else {
            return cap_null_cap_new();
        }
    }
#endif
    default:
        return cap;
    }
}

cap_t CONST
Arch_maskCapRights(seL4_CapRights_t cap_rights_mask, cap_t cap)
{
    if (cap_get_capType(cap) == cap_frame_cap) {
        vm_rights_t vm_rights;

        vm_rights = vmRightsFromWord(cap_frame_cap_get_capFVMRights(cap));
        vm_rights = maskVMRights(vm_rights, cap_rights_mask);

        return cap_frame_cap_set_capFVMRights(cap, wordFromVMRights(vm_rights));
    } else {
        return cap;
    }
}

finaliseCap_ret_t
Arch_finaliseCap(cap_t cap, bool_t final)
{
    finaliseCap_ret_t fc_ret;

    switch (cap_get_capType(cap)) {
    case cap_asid_pool_cap:
        if (final) {
            deleteASIDPool(cap_asid_pool_cap_get_capASIDBase(cap),
                           ASID_POOL_PTR(cap_asid_pool_cap_get_capASIDPool(cap)));
        }
        break;

    case cap_page_global_directory_cap:
        if (final && cap_page_global_directory_cap_get_capPGDIsMapped(cap)) {
            deleteASID(cap_page_global_directory_cap_get_capPGDMappedASID(cap),
                       PGDE_PTR(cap_page_global_directory_cap_get_capPGDBasePtr(cap)));
        }
        break;

    case cap_page_upper_directory_cap:
        if (final && cap_page_upper_directory_cap_get_capPUDIsMapped(cap)) {
#if defined(CONFIG_ARM_HYPERVISOR_SUPPORT) && defined(CONFIG_START_L1)
            deleteASID(cap_page_upper_directory_cap_get_capPUDMappedASID(cap),
                       PGDE_PTR(cap_page_upper_directory_cap_get_capPUDBasePtr(cap)));
#else
            unmapPageUpperDirectory(cap_page_upper_directory_cap_get_capPUDMappedASID(cap),
                                    cap_page_upper_directory_cap_get_capPUDMappedAddress(cap),
                                    PUDE_PTR(cap_page_upper_directory_cap_get_capPUDBasePtr(cap)));
#endif
        }
        break;

    case cap_page_directory_cap:
        if (final && cap_page_directory_cap_get_capPDIsMapped(cap)) {
            unmapPageDirectory(cap_page_directory_cap_get_capPDMappedASID(cap),
                               cap_page_directory_cap_get_capPDMappedAddress(cap),
                               PDE_PTR(cap_page_directory_cap_get_capPDBasePtr(cap)));
        }
        break;

    case cap_page_table_cap:
        if (final && cap_page_table_cap_get_capPTIsMapped(cap)) {
            unmapPageTable(cap_page_table_cap_get_capPTMappedASID(cap),
                           cap_page_table_cap_get_capPTMappedAddress(cap),
                           PTE_PTR(cap_page_table_cap_get_capPTBasePtr(cap)));
        }
        break;

    case cap_frame_cap:
        if (cap_frame_cap_get_capFMappedASID(cap)) {
#ifdef CONFIG_ARM_SMMU
            if (isIOSpaceFrameCap(cap)) {
                unmapIOPage(cap);
                break;
            }
#endif

            unmapPage(cap_frame_cap_get_capFSize(cap),
                      cap_frame_cap_get_capFMappedASID(cap),
                      cap_frame_cap_get_capFMappedAddress(cap),
                      cap_frame_cap_get_capFBasePtr(cap));
        }
        break;

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case cap_vcpu_cap:
        if (final) {
            vcpu_finalise(VCPU_PTR(cap_vcpu_cap_get_capVCPUPtr(cap)));
        }
        break;
#endif

#ifdef CONFIG_ARM_SMMU
    case cap_io_space_cap:
        if (final) {
            clearIOPageDirectory(cap);
        }
        break;

    case cap_io_page_table_cap:
        if (final && cap_io_page_table_cap_get_capIOPTIsMapped(cap)) {
            deleteIOPageTable(cap);
        }
        break;
#endif
    }

    fc_ret.remainder = cap_null_cap_new();
    fc_ret.cleanupInfo = cap_null_cap_new();
    return fc_ret;
}

bool_t CONST Arch_sameRegionAs(cap_t cap_a, cap_t cap_b)
{
    switch (cap_get_capType(cap_a)) {
    case cap_frame_cap:
        if (cap_get_capType(cap_b) == cap_frame_cap) {

            word_t botA, botB, topA, topB;
            botA = cap_frame_cap_get_capFBasePtr(cap_a);
            botB = cap_frame_cap_get_capFBasePtr(cap_b);
            topA = botA + MASK(pageBitsForSize(cap_frame_cap_get_capFSize(cap_a)));
            topB = botB + MASK(pageBitsForSize(cap_frame_cap_get_capFSize(cap_b))) ;
            return ((botA <= botB) && (topA >= topB) && (botB <= topB));
        }
        break;

    case cap_page_table_cap:
        if (cap_get_capType(cap_b) == cap_page_table_cap) {
            return cap_page_table_cap_get_capPTBasePtr(cap_a) ==
                   cap_page_table_cap_get_capPTBasePtr(cap_b);
        }
        break;

    case cap_page_directory_cap:
        if (cap_get_capType(cap_b) == cap_page_directory_cap) {
            return cap_page_directory_cap_get_capPDBasePtr(cap_a) ==
                   cap_page_directory_cap_get_capPDBasePtr(cap_b);
        }
        break;

    case cap_page_upper_directory_cap:
        if (cap_get_capType(cap_b) == cap_page_upper_directory_cap) {
            return cap_page_upper_directory_cap_get_capPUDBasePtr(cap_a) ==
                   cap_page_upper_directory_cap_get_capPUDBasePtr(cap_b);
        }
        break;

    case cap_page_global_directory_cap:
        if (cap_get_capType(cap_b) == cap_page_global_directory_cap) {
            return cap_page_global_directory_cap_get_capPGDBasePtr(cap_a) ==
                   cap_page_global_directory_cap_get_capPGDBasePtr(cap_b);
        }
        break;

    case cap_asid_control_cap:
        if (cap_get_capType(cap_b) == cap_asid_control_cap) {
            return true;
        }
        break;

    case cap_asid_pool_cap:
        if (cap_get_capType(cap_b) == cap_asid_pool_cap) {
            return cap_asid_pool_cap_get_capASIDPool(cap_a) ==
                   cap_asid_pool_cap_get_capASIDPool(cap_b);
        }
        break;

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case cap_vcpu_cap:
        if (cap_get_capType(cap_b) == cap_vcpu_cap) {
            return cap_vcpu_cap_get_capVCPUPtr(cap_a) ==
                   cap_vcpu_cap_get_capVCPUPtr(cap_b);
        }
        break;
#endif
#ifdef CONFIG_ARM_SMMU
#ifdef CONFIG_ARM_SMMU_V2
    case cap_io_space_cap:
        if (cap_get_capType(cap_b) == cap_io_space_cap) {
            return cap_io_space_cap_get_capStreamID(cap_a) ==
                   cap_io_space_cap_get_capStreamID(cap_b);
        }
        break;
#endif
    case cap_io_page_table_cap:
        if (cap_get_capType(cap_b) == cap_io_page_table_cap) {
            return cap_io_page_table_cap_get_capIOPTBasePtr(cap_a) ==
                   cap_io_page_table_cap_get_capIOPTBasePtr(cap_b);
        }
        break;
#endif
    }

    return false;
}

bool_t CONST
Arch_sameObjectAs(cap_t cap_a, cap_t cap_b)
{
    if (cap_get_capType(cap_a) == cap_frame_cap) {
        if (cap_get_capType(cap_b) == cap_frame_cap) {
            return ((cap_frame_cap_get_capFBasePtr(cap_a) ==
                     cap_frame_cap_get_capFBasePtr(cap_b)) &&
                    (cap_frame_cap_get_capFSize(cap_a) ==
                     cap_frame_cap_get_capFSize(cap_b)) &&
                    ((cap_frame_cap_get_capFIsDevice(cap_a) == 0) ==
                     (cap_frame_cap_get_capFIsDevice(cap_b) == 0)));
        }
    }
    return Arch_sameRegionAs(cap_a, cap_b);
}

word_t
Arch_getObjectSize(word_t t)
{
    switch (t) {
    case seL4_ARM_SmallPageObject:
        return ARMSmallPageBits;
    case seL4_ARM_LargePageObject:
        return ARMLargePageBits;
    case seL4_ARM_HugePageObject:
        return ARMHugePageBits;
    case seL4_ARM_PageTableObject:
        return seL4_PageTableBits;
    case seL4_ARM_PageDirectoryObject:
        return seL4_PageDirBits;
    case seL4_ARM_PageUpperDirectoryObject:
        return seL4_PUDBits;
    case seL4_ARM_PageGlobalDirectoryObject:
        return seL4_PGDBits;
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case seL4_ARM_VCPUObject:
        return VCPU_SIZE_BITS;
#endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */
#ifdef CONFIG_ARM_SMMU
    case seL4_ARM_IOPageTableObject:
        return seL4_IOPageTableBits;
#endif
    default:
        fail("Invalid object type");
        return 0;
    }
}

cap_t
Arch_createObject(object_t t, void *regionBase, word_t userSize, bool_t deviceMemory)
{
    switch (t) {
    case seL4_ARM_SmallPageObject:
        return cap_frame_cap_new(
                   asidInvalid,           /* capFMappedASID */
                   (word_t)regionBase,    /* capFBasePtr */
                   ARMSmallPage,          /* capFSize */
                   0,                     /* capFMappedAddress */
                   VMReadWrite,           /* capFVMRights */
#ifdef CONFIG_ARM_SMMU
                   0,                     /* capFIsIOSpace */
#endif
                   !!deviceMemory         /* capFIsDevice */
               );

    case seL4_ARM_LargePageObject:
        return cap_frame_cap_new(
                   asidInvalid,           /* capFMappedASID */
                   (word_t)regionBase,    /* capFBasePtr */
                   ARMLargePage,          /* capFSize */
                   0,                     /* capFMappedAddress */
                   VMReadWrite,           /* capFVMRights */
#ifdef CONFIG_ARM_SMMU
                   0,                     /* capFIsIOSpace */
#endif
                   !!deviceMemory         /* capFIsDevice */
               );

    case seL4_ARM_HugePageObject:
        return cap_frame_cap_new(
                   asidInvalid,           /* capFMappedASID */
                   (word_t)regionBase,    /* capFBasePtr */
                   ARMHugePage,           /* capFSize */
                   0,                     /* capFMappedAddress */
                   VMReadWrite,           /* capFVMRights */
#ifdef CONFIG_ARM_SMMU
                   0,                     /* capFIsIOSpace */
#endif
                   !!deviceMemory         /* capFIsDevice */
               );

    case seL4_ARM_PageGlobalDirectoryObject:
        return cap_page_global_directory_cap_new(
                   asidInvalid,           /* capPGDMappedASID   */
                   (word_t)regionBase,    /* capPGDBasePtr      */
                   0                      /* capPGDIsMapped     */
               );

    case seL4_ARM_PageUpperDirectoryObject:
        return cap_page_upper_directory_cap_new(
                   asidInvalid,           /* capPUDMappedASID    */
                   (word_t)regionBase,    /* capPUDBasePtr       */
                   0,                     /* capPUDIsMapped      */
                   0                      /* capPUDMappedAddress */
               );

    case seL4_ARM_PageDirectoryObject:
        return cap_page_directory_cap_new(
                   asidInvalid,           /* capPDMappedASID    */
                   (word_t)regionBase,    /* capPDBasePtr       */
                   0,                     /* capPDIsMapped      */
                   0                      /* capPDMappedAddress */
               );

    case seL4_ARM_PageTableObject:
        return cap_page_table_cap_new(
                   asidInvalid,           /* capPTMappedASID    */
                   (word_t)regionBase,    /* capPTBasePtr       */
                   0,                     /* capPTIsMapped      */
                   0                      /* capPTMappedAddress */
               );

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case seL4_ARM_VCPUObject:
        vcpu_init(VCPU_PTR(regionBase));
        return cap_vcpu_cap_new(VCPU_REF(regionBase));
#endif

#ifdef CONFIG_ARM_SMMU
    case seL4_ARM_IOPageTableObject:
        return cap_io_page_table_cap_new(
                   0,                  /* capIOPTIsMapped      */
                   asidInvalid,        /* capIOPTASID          */
                   0xF,                /* capIOPTLevel         */
                   (word_t)regionBase, /* capIOPTBasePtr       */
                   0                   /* capIOPTMappedAddress */
               );
#endif
    default:
        fail("Arch_createObject got an API type or invalid object type");
    }
}

exception_t
Arch_decodeInvocation(word_t label, word_t length, cptr_t cptr,
                      cte_t *slot, cap_t cap, extra_caps_t extraCaps,
                      bool_t call, word_t *buffer)
{
    /* The C parser cannot handle a switch statement with only a default
     * case. So we need to do some gymnastics to remove the switch if
     * there are no other cases */
#if defined(CONFIG_ARM_HYPERVISOR_SUPPORT) ||  defined(CONFIG_ARM_SMMU)
    switch (cap_get_capType(cap)) {
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    case cap_vcpu_cap:
        return decodeARMVCPUInvocation(label, length, cptr, slot, cap, extraCaps, call, buffer);
#endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */
#ifdef CONFIG_ARM_SMMU
    case cap_io_space_cap:
        return decodeARMIOSpaceInvocation(label, cap);
    case cap_io_page_table_cap:
        return decodeARMIOPTInvocation(label, length, slot, cap, extraCaps, buffer);
#endif
    default:
        break;
#else
    {
#endif
    }

    return decodeARMMMUInvocation(label, length, cptr, slot, cap, extraCaps, buffer);
}

void
Arch_prepareThreadDelete(tcb_t *thread)
{
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    if (thread->tcbArch.tcbVCPU) {
        dissociateVCPUTCB(thread->tcbArch.tcbVCPU, thread);
    }
#else  /* CONFIG_ARM_HYPERVISOR_SUPPORT */
    /* No action required on ARM. */
#endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */

}
