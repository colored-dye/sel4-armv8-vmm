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
#include <benchmark/benchmark.h>
#include <api/failures.h>
#include <api/syscall.h>
#include <kernel/boot.h>
#include <kernel/cspace.h>
#include <kernel/thread.h>
#include <kernel/stack.h>
#include <machine/io.h>
#include <machine/debug.h>
#include <model/statedata.h>
#include <object/cnode.h>
#include <object/untyped.h>
#include <arch/api/invocation.h>
#include <arch/kernel/vspace.h>
#include <linker.h>
#include <plat/machine/devices.h>
#include <plat/machine/hardware.h>
#include <armv/context_switch.h>
#include <arch/object/iospace.h>
#include <arch/object/vcpu.h>
#include <arch/machine/tlb.h>

/*
 * Memory types are defined in Memory Attribute Indirection Register.
 *  - nGnRnE Device non-Gathering, non-Reordering, No Early write acknowledgement
 *  - nGnRE Unused Device non-Gathering, non-Reordering, Early write acknowledgement
 *  - GRE Unused Device Gathering, Reordering, Early write acknowledgement
 *  - NORMAL_NC Normal Memory, Inner/Outer non-cacheable
 *  - NORMAL Normal Memory, Inner/Outer Write-back non-transient, Write-allocate, Read-allocate
 * Note: These should match with contents of MAIR_EL1 register!
 */
enum mair_types {
    DEVICE_nGnRnE = 0,
    DEVICE_nGnRE = 1,
    DEVICE_GRE = 2,
    NORMAL_NC = 3,
    NORMAL = 4
};

/* Leif from Linaro said the big.LITTLE clusters should be treated as
 * inner shareable, and we believe so, although the Example B2-1 given in
 * ARM ARM DDI 0487B.b (ID092517) says otherwise.
 */

#define SMP_SHARE   3

/*
 * Stage 2 Translations have differing attributes than Stage 1 Translations.
 * These style of translations no longer use the MAIR_EL1 registers to
 * determine the state of the memory region
 *
 * See ARMv8 D4.3 for more information. The specific bits listed below will
 * go to the MemAttr bits of the Lower Attributes.
 */
enum mair_s2_types {
    S2_DEVICE_nGnRnE = 0b0000,
    S2_DEVICE_nGnRE = 0b0001,
    S2_DEVICE_nGRE  = 0b0010,
    S2_DEVICE_GRE = 0b0011,
    S2_NORMAL_INNER_NC_OUTER_NC = 0b0101,
    S2_NORMAL_INNER_WTC_OUTER_NC = 0b0110,
    S2_NORMAL_INNER_WBC_OUTER_NC = 0b0111,

    S2_NORMAL_INNER_NC_OUTER_WTC = 0b1001,
    S2_NORMAL_INNER_WTC_OUTER_WTC = 0b1010,
    S2_NORMAL_INNER_WBC_OUTER_WTC = 0b1011,

    S2_NORMAL_INNER_NC_OUTER_WBC = 0b1101,
    S2_NORMAL_INNER_WTC_OUTER_WBC = 0b1110,
    S2_NORMAL_INNER_WBC_OUTER_WBC = 0b1111,

    S2_NORMAL = S2_NORMAL_INNER_WBC_OUTER_WBC
};

struct lookupPGDSlot_ret {
    exception_t status;
    pgde_t *pgdSlot;
};
typedef struct lookupPGDSlot_ret lookupPGDSlot_ret_t;

struct lookupPUDSlot_ret {
    exception_t status;
    pude_t *pudSlot;
};
typedef struct lookupPUDSlot_ret lookupPUDSlot_ret_t;

struct lookupPDSlot_ret {
    exception_t status;
    pde_t *pdSlot;
};
typedef struct lookupPDSlot_ret lookupPDSlot_ret_t;

struct lookupPTSlot_ret {
    exception_t status;
    pte_t *ptSlot;
};
typedef struct lookupPTSlot_ret lookupPTSlot_ret_t;

struct lookupFrame_ret {
    paddr_t frameBase;
    vm_page_size_t frameSize;
    bool_t valid;
};
typedef struct lookupFrame_ret lookupFrame_ret_t;

struct findVSpaceForASID_ret {
    exception_t status;
    vspace_root_t *vspace_root;
};
typedef struct findVSpaceForASID_ret findVSpaceForASID_ret_t;

static word_t CONST
APFromVMRights(vm_rights_t vm_rights)
{
    switch (vm_rights) {
    case VMKernelOnly:
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            return 0;
        } else {
            return 0;
        }

    case VMReadWrite:
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            return 3;
        } else {
            return 1;
        }

    case VMKernelReadOnly:
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            /* no corresponding AP for S2AP */
            return 0;
        } else {
            return 2;
        }

    case VMReadOnly:
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            return 1;
        } else {
            return 3;
        }

    default:
        fail("Invalid VM rights");
    }
}

vm_rights_t CONST
maskVMRights(vm_rights_t vm_rights, seL4_CapRights_t cap_rights_mask)
{
    if (vm_rights == VMReadOnly &&
            seL4_CapRights_get_capAllowRead(cap_rights_mask)) {
        return VMReadOnly;
    }
    if (vm_rights == VMReadWrite &&
            seL4_CapRights_get_capAllowRead(cap_rights_mask)) {
        if (!seL4_CapRights_get_capAllowWrite(cap_rights_mask)) {
            return VMReadOnly;
        } else {
            return VMReadWrite;
        }
    }
    if (vm_rights == VMReadWrite &&
            !seL4_CapRights_get_capAllowRead(cap_rights_mask) &&
            seL4_CapRights_get_capAllowWrite(cap_rights_mask)) {
        userError("Attempted to make unsupported write only mapping");
    }
    return VMKernelOnly;
}

struct vspace_starting_levels vspace_sl =
{
#ifndef CONFIG_START_L1
    .top_cap = cap_page_global_directory_cap,
    .top_cap_mapped = cap_page_global_directory_cap_get_capPGDIsMapped,
    .top_cap_asid = cap_page_global_directory_cap_get_capPGDMappedASID,
    .top_base_ptr = cap_page_global_directory_cap_get_capPGDBasePtr,
    .top_cap_ptr_map = cap_page_global_directory_cap_ptr_set_capPGDMappedASID,
    .top_cap_ptr_mapped = cap_page_global_directory_cap_ptr_set_capPGDIsMapped
#else
    .top_cap = cap_page_upper_directory_cap,
    .top_cap_mapped = cap_page_upper_directory_cap_get_capPUDIsMapped,
    .top_cap_asid = cap_page_upper_directory_cap_get_capPUDMappedASID,
    .top_base_ptr = cap_page_upper_directory_cap_get_capPUDBasePtr,
    .top_cap_ptr_map = cap_page_upper_directory_cap_ptr_set_capPUDMappedASID,
    .top_cap_ptr_mapped = cap_page_upper_directory_cap_ptr_set_capPUDIsMapped
#endif
};

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define pdeS2_new pteS2_new
#define pdeS2_t   pteS2_t
#define table_t   pteS2_t
#define dir_t     pdeS2_t
#else
#define table_t   pte_t
#define dir_t     pde_t
#endif

/* ==================== BOOT CODE STARTS HERE ==================== */

BOOT_CODE void
map_kernel_frame(paddr_t paddr, pptr_t vaddr, vm_rights_t vm_rights, vm_attributes_t attributes)
{
    assert(vaddr >= PPTR_TOP);

    if (vm_attributes_get_armPageCacheable(attributes)) {
        armKSGlobalKernelPT[GET_PT_INDEX(vaddr)] = pte_new(
                                                       1,                          /* unprivileged execute never */
                                                       paddr,
                                                       0,                          /* global */
                                                       1,                          /* access flag */
                                                       SMP_TERNARY(SMP_SHARE, 0),          /* Inner-shareable if SMP enabled, otherwise unshared */
                                                       APFromVMRights(vm_rights),
                                                       NORMAL,
                                                       0b11                        /* reserved */
                                                   );
    } else {
        armKSGlobalKernelPT[GET_PT_INDEX(vaddr)] = pte_new(
                                                       1,                          /* unprivileged execute never */
                                                       paddr,
                                                       0,                          /* global */
                                                       1,                          /* access flag */
                                                       0,                          /* Ignored - Outter shareable */
                                                       APFromVMRights(vm_rights),
                                                       DEVICE_nGnRnE,
                                                       0b11                        /* reserved */
                                                   );
    }
}

BOOT_CODE void
map_kernel_window(void)
{

    paddr_t paddr;
    pptr_t vaddr;
    word_t idx;

    /* verify that the kernel window as at the last entry of the PGD */
    assert(GET_PGD_INDEX(kernelBase) == BIT(PGD_INDEX_BITS) - 1);
    assert(IS_ALIGNED(kernelBase, seL4_LargePageBits));
    /* verify that the kernel device window is 1gb aligned and 1gb in size */
    assert(GET_PUD_INDEX(PPTR_TOP) == BIT(PUD_INDEX_BITS) - 1);
    assert(IS_ALIGNED(PPTR_TOP, seL4_HugePageBits));

    /* place the PUD into the PGD */
    armKSGlobalKernelPGD[GET_PGD_INDEX(kernelBase)] = pgde_new(
                                                          pptr_to_paddr(armKSGlobalKernelPUD),
                                                          0b11  /* reserved */
                                                      );

    /* place all PDs except the last one in PUD */
    for (idx = GET_PUD_INDEX(kernelBase); idx < GET_PUD_INDEX(PPTR_TOP); idx++) {
        armKSGlobalKernelPUD[idx] = pude_pude_pd_new(
                                        pptr_to_paddr(&armKSGlobalKernelPDs[idx][0])
                                    );
    }

    /* map the kernel window using large pages */
    vaddr = kernelBase;
    for (paddr = physBase; paddr < PADDR_TOP; paddr += BIT(seL4_LargePageBits)) {
        armKSGlobalKernelPDs[GET_PUD_INDEX(vaddr)][GET_PD_INDEX(vaddr)] = pde_pde_large_new(
                                                                              !config_set(CONFIG_ARM_HYPERVISOR_SUPPORT), /* UXN only for when S1 supports 2 Ranges */
                                                                              paddr,
                                                                              0,                        /* global */
                                                                              1,                        /* access flag */
                                                                              SMP_TERNARY(SMP_SHARE, 0),        /* Inner-shareable if SMP enabled, otherwise unshared */
                                                                              0,                        /* VMKernelOnly */
                                                                              NORMAL
                                                                          );
        vaddr += BIT(seL4_LargePageBits);
    }

    /* put the PD into the PUD for device window */
    armKSGlobalKernelPUD[GET_PUD_INDEX(PPTR_TOP)] = pude_pude_pd_new(
                                                        pptr_to_paddr(&armKSGlobalKernelPDs[BIT(PUD_INDEX_BITS) - 1][0])
                                                    );

    /* put the PT into the PD for device window */
    armKSGlobalKernelPDs[BIT(PUD_INDEX_BITS) - 1][BIT(PD_INDEX_BITS) - 1] = pde_pde_small_new(
                                                                                pptr_to_paddr(armKSGlobalKernelPT)
                                                                            );

    map_kernel_devices();
}

static BOOT_CODE void
map_it_frame_cap(cap_t vspace_cap, cap_t frame_cap, bool_t executable)
{
    pude_t *pud;
    pde_t *pd;
    table_t *pt;

    vptr_t vptr = cap_frame_cap_get_capFMappedAddress(frame_cap);
    void *pptr = (void*)cap_frame_cap_get_capFBasePtr(frame_cap);

    assert(cap_frame_cap_get_capFMappedASID(frame_cap) != 0);

#ifndef CONFIG_START_L1
    pgde_t *pgd = PGD_PTR(pptr_of_cap(vspace_cap));
    pgd += GET_PGD_INDEX(vptr);
    assert(pgde_ptr_get_present(pgd));
    pud = paddr_to_pptr(pgde_ptr_get_pud_base_address(pgd));
#else
    pud = PUD_PTR(pptr_of_cap(vspace_cap));
#endif

    pud += GET_PUD_INDEX(vptr);
    assert(pude_pude_pd_ptr_get_present(pud));
    pd = paddr_to_pptr(pude_pude_pd_ptr_get_pd_base_address(pud));
    pd += GET_PD_INDEX(vptr);
    assert(pde_pde_small_ptr_get_present(pd));
    pt = paddr_to_pptr(pde_pde_small_ptr_get_pt_base_address(pd));

#ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
    *(pt + GET_PT_INDEX(vptr)) = pte_new(
                                     !executable,                    /* unprivileged execute never */
                                     pptr_to_paddr(pptr),            /* page_base_address    */
                                     1,                              /* not global */
                                     1,                              /* access flag */
                                     SMP_TERNARY(SMP_SHARE, 0),              /* Inner-shareable if SMP enabled, otherwise unshared */
                                     APFromVMRights(VMReadWrite),
                                     NORMAL,
                                     0b11                            /* reserved */
                                 );
#else
    *(pt + GET_PT_INDEX(vptr)) = pteS2_new(
                                     (!executable & 0x1) << 1,       /* XN[1] == 1 -> No execution, 0 -> Execution @ EL1 and EL0 */
                                     0,                              /* Non-Contiguous */
                                     0,                              /* Diry-Bit */
                                     pptr_to_paddr(pptr),            /* page_base_address */
                                     1,                              /* access flag */
                                     0,                              /* Non-Shareable */
                                     APFromVMRights(VMReadWrite),
                                     S2_NORMAL,
                                     0b11                            /* 4kB Page */
                                 );
#endif
}

static BOOT_CODE cap_t
create_it_frame_cap(pptr_t pptr, vptr_t vptr, asid_t asid, bool_t use_large)
{
    vm_page_size_t frame_size;
    if (use_large) {
        frame_size = ARMLargePage;
    } else {
        frame_size = ARMSmallPage;
    }
    return
        cap_frame_cap_new(
            asid,                          /* capFMappedASID */
            pptr,                          /* capFBasePtr */
            frame_size,                    /* capFSize */
            vptr,                          /* capFMappedAddress */
            wordFromVMRights(VMReadWrite), /* capFVMRights */
#ifdef CONFIG_ARM_SMMU
            false,                         /* IOSpace */
#endif
            false                          /* capFIsDevice */
        );
}

static BOOT_CODE void
map_it_pt_cap(cap_t vspace_cap, cap_t pt_cap)
{
    pude_t *pud;
    pde_t *pd;
    pte_t *pt = PT_PTR(cap_page_table_cap_get_capPTBasePtr(pt_cap));
    vptr_t vptr = cap_page_table_cap_get_capPTMappedAddress(pt_cap);

#ifndef CONFIG_START_L1
    pgde_t *pgd = PGD_PTR(pptr_of_cap(vspace_cap));
    pgd += GET_PGD_INDEX(vptr);
    assert(pgde_ptr_get_present(pgd));
    pud = paddr_to_pptr(pgde_ptr_get_pud_base_address(pgd));
#else
    pud = PUD_PTR(pptr_of_cap(vspace_cap));
#endif

    assert(cap_page_table_cap_get_capPTIsMapped(pt_cap));

    pud += GET_PUD_INDEX(vptr);
    assert(pude_pude_pd_ptr_get_present(pud));
    pd = paddr_to_pptr(pude_pude_pd_ptr_get_pd_base_address(pud));
    *(pd + GET_PD_INDEX(vptr)) = pde_pde_small_new(
                                     pptr_to_paddr(pt)
                                 );
}

static BOOT_CODE cap_t
create_it_pt_cap(cap_t vspace_cap, pptr_t pptr, vptr_t vptr, asid_t asid)
{
    cap_t cap;
    cap = cap_page_table_cap_new(
              asid,                   /* capPTMappedASID */
              pptr,                   /* capPTBasePtr */
              1,                      /* capPTIsMapped */
              vptr                    /* capPTMappedAddress */
          );
    map_it_pt_cap(vspace_cap, cap);
    return cap;
}

static BOOT_CODE void
map_it_pd_cap(cap_t vspace_cap, cap_t pd_cap)
{
    pude_t *pud;

    pde_t *pd = PD_PTR(cap_page_directory_cap_get_capPDBasePtr(pd_cap));
    vptr_t vptr = cap_page_directory_cap_get_capPDMappedAddress(pd_cap);

#ifndef CONFIG_START_L1
    pgde_t *pgd = PGD_PTR(pptr_of_cap(vspace_cap));
    pgd += GET_PGD_INDEX(vptr);
    assert(pgde_ptr_get_present(pgd));
    pud = paddr_to_pptr(pgde_ptr_get_pud_base_address(pgd));
#else
    pud = PUD_PTR(pptr_of_cap(vspace_cap));
#endif

    assert(cap_page_directory_cap_get_capPDIsMapped(pd_cap));

    *(pud + GET_PUD_INDEX(vptr)) = pude_pude_pd_new(
                                       pptr_to_paddr(pd)
                                   );
}

static BOOT_CODE cap_t
create_it_pd_cap(cap_t vspace_cap, pptr_t pptr, vptr_t vptr, asid_t asid)
{
    cap_t cap;
    cap = cap_page_directory_cap_new(
              asid,                   /* capPDMappedASID */
              pptr,                   /* capPDBasePtr */
              1,                      /* capPDIsMapped */
              vptr                    /* capPDMappedAddress */
          );
    map_it_pd_cap(vspace_cap, cap);
    return cap;
}

#ifndef CONFIG_START_L1

static BOOT_CODE void
map_it_pud_cap(cap_t vspace_cap, cap_t pud_cap)
{
    pgde_t *pgd = PGD_PTR(pptr_of_cap(vspace_cap));
    pude_t *pud = PUD_PTR(cap_page_upper_directory_cap_get_capPUDBasePtr(pud_cap));
    vptr_t vptr = cap_page_upper_directory_cap_get_capPUDMappedAddress(pud_cap);

    assert(cap_page_upper_directory_cap_get_capPUDIsMapped(pud_cap));

    *(pgd + GET_PGD_INDEX(vptr)) = pgde_new(
                                       pptr_to_paddr(pud),
                                       0b11                        /* reserved */
                                   );
}

static BOOT_CODE cap_t
create_it_pud_cap(cap_t vspace_cap, pptr_t pptr, vptr_t vptr, asid_t asid)
{
    cap_t cap;
    cap = cap_page_upper_directory_cap_new(
              asid,               /* capPUDMappedASID */
              pptr,               /* capPUDBasePtr */
              1,                  /* capPUDIsMapped */
              vptr                /* capPUDMappedAddress */
          );
    map_it_pud_cap(vspace_cap, cap);
    return cap;
}

#endif

BOOT_CODE cap_t
create_it_address_space(cap_t root_cnode_cap, v_region_t it_v_reg)
{
    cap_t      vspace_cap;
    vptr_t     vptr;
    pptr_t     pptr;
    seL4_SlotPos slot_pos_before;
    seL4_SlotPos slot_pos_after;

#ifndef CONFIG_START_L1
    /* create the PGD */
    pptr = alloc_region(seL4_PGDBits);
    if (!pptr) {
        return cap_null_cap_new();
    }
    memzero(PGD_PTR(pptr), BIT(seL4_PGDBits));
    vspace_cap = cap_page_global_directory_cap_new(
                     IT_ASID,        /* capPGDMappedASID */
                     pptr,           /* capPGDBasePtr   */
                     1               /* capPGDIsMapped   */
                 );
    slot_pos_before = ndks_boot.slot_pos_cur;
    write_slot(SLOT_PTR(pptr_of_cap(root_cnode_cap), seL4_CapInitThreadVSpace), vspace_cap);

    /* Create any PUDs needed for the user land image */
    for (vptr = ROUND_DOWN(it_v_reg.start, PGD_INDEX_OFFSET);
            vptr < it_v_reg.end;
            vptr += BIT(PGD_INDEX_OFFSET)) {
        pptr = alloc_region(seL4_PUDBits);
        if (!pptr) {
            return cap_null_cap_new();
        }
        memzero(PUD_PTR(pptr), BIT(seL4_PUDBits));
        if (!provide_cap(root_cnode_cap, create_it_pud_cap(vspace_cap, pptr, vptr, IT_ASID))) {
            return cap_null_cap_new();
        }
    }

#else
    /* create the PUD */
    pptr = alloc_region(seL4_PUDBits);
    if (!pptr) {
        return cap_null_cap_new();
    }
    memzero(PUD_PTR(pptr), BIT(seL4_PUDBits));
    vspace_cap = cap_page_upper_directory_cap_new(
                     IT_ASID,        /* capPUDMappedASID */
                     pptr,           /* capPUDBasePtr   */
                     1,              /* capPUDIsMapped   */
                     0
                 );
    slot_pos_before = ndks_boot.slot_pos_cur;
    write_slot(SLOT_PTR(pptr_of_cap(root_cnode_cap), seL4_CapInitThreadVSpace), vspace_cap);

#endif
    /* Create any PDs needed for the user land image */
    for (vptr = ROUND_DOWN(it_v_reg.start, PUD_INDEX_OFFSET);
            vptr < it_v_reg.end;
            vptr += BIT(PUD_INDEX_OFFSET)) {
        pptr = alloc_region(seL4_PageDirBits);
        if (!pptr) {
            return cap_null_cap_new();
        }
        memzero(PD_PTR(pptr), BIT(seL4_PageDirBits));
        if (!provide_cap(root_cnode_cap, create_it_pd_cap(vspace_cap, pptr, vptr, IT_ASID))) {
            return cap_null_cap_new();
        }
    }

    /* Create any PTs needed for the user land image */
    for (vptr = ROUND_DOWN(it_v_reg.start, PD_INDEX_OFFSET);
            vptr < it_v_reg.end;
            vptr += BIT(PD_INDEX_OFFSET)) {
        pptr = alloc_region(seL4_PageTableBits);
        if (!pptr) {
            return cap_null_cap_new();
        }
        memzero(PT_PTR(pptr), BIT(seL4_PageTableBits));
        if (!provide_cap(root_cnode_cap, create_it_pt_cap(vspace_cap, pptr, vptr, IT_ASID))) {
            return cap_null_cap_new();
        }
    }

    slot_pos_after = ndks_boot.slot_pos_cur;
    ndks_boot.bi_frame->userImagePaging = (seL4_SlotRegion) {
        slot_pos_before, slot_pos_after
    };
    return vspace_cap;
}

BOOT_CODE cap_t
create_unmapped_it_frame_cap(pptr_t pptr, bool_t use_large)
{
    return create_it_frame_cap(pptr, 0, asidInvalid, use_large);
}

BOOT_CODE cap_t
create_mapped_it_frame_cap(cap_t pd_cap, pptr_t pptr, vptr_t vptr, asid_t asid, bool_t use_large, bool_t executable)
{
    cap_t cap = create_it_frame_cap(pptr, vptr, asid, use_large);
    map_it_frame_cap(pd_cap, cap, executable);
    return cap;
}

BOOT_CODE void
activate_kernel_vspace(void)
{
    cleanInvalidateL1Caches();
    setCurrentKernelVSpaceRoot(ttbr_new(0, pptr_to_paddr(armKSGlobalKernelPGD)));

    /* Prevent elf-loader address translation to fill up TLB */
    setCurrentUserVSpaceRoot(ttbr_new(0, pptr_to_paddr(armKSGlobalUserPGD)));

    invalidateLocalTLB();
    lockTLBEntry(kernelBase);
}

BOOT_CODE void
write_it_asid_pool(cap_t it_ap_cap, cap_t it_vspace_cap)
{
    asid_pool_t* ap = ASID_POOL_PTR(pptr_of_cap(it_ap_cap));
    ap->array[IT_ASID] = (void *)(pptr_of_cap(it_vspace_cap));
    armKSASIDTable[IT_ASID >> asidLowBits] = ap;
}

/* ==================== BOOT CODE FINISHES HERE ==================== */

static findVSpaceForASID_ret_t
findVSpaceForASID(asid_t asid)
{
    findVSpaceForASID_ret_t ret;
    asid_pool_t *poolPtr;
    vspace_root_t *vspace_root;

    poolPtr = armKSASIDTable[asid >> asidLowBits];
    if (!poolPtr) {
        current_lookup_fault = lookup_fault_invalid_root_new();

        ret.vspace_root = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
        return ret;
    }

    vspace_root = poolPtr->array[asid & MASK(asidLowBits)];
    if (!vspace_root) {
        current_lookup_fault = lookup_fault_invalid_root_new();

        ret.vspace_root = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
        return ret;
    }

    ret.vspace_root = vspace_root;
    ret.status = EXCEPTION_NONE;
    return ret;
}

word_t * PURE
lookupIPCBuffer(bool_t isReceiver, tcb_t *thread)
{
    word_t w_bufferPtr;
    cap_t bufferCap;
    vm_rights_t vm_rights;

    w_bufferPtr = thread->tcbIPCBuffer;
    bufferCap = TCB_PTR_CTE_PTR(thread, tcbBuffer)->cap;

    if (unlikely(cap_get_capType(bufferCap) != cap_frame_cap)) {
        return NULL;
    }
    if (unlikely(cap_frame_cap_get_capFIsDevice(bufferCap))) {
        return NULL;
    }

    vm_rights = cap_frame_cap_get_capFVMRights(bufferCap);
    if (likely(vm_rights == VMReadWrite ||
               (!isReceiver && vm_rights == VMReadOnly))) {
        word_t basePtr;
        unsigned int pageBits;

        basePtr = cap_frame_cap_get_capFBasePtr(bufferCap);
        pageBits = pageBitsForSize(cap_frame_cap_get_capFSize(bufferCap));
        return (word_t *)(basePtr + (w_bufferPtr & MASK(pageBits)));
    } else {
        return NULL;
    }
}

exception_t
checkValidIPCBuffer(vptr_t vptr, cap_t cap)
{
    if (cap_get_capType(cap) != cap_frame_cap) {
        userError("IPC Buffer is an invalid cap.");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(cap_frame_cap_get_capFIsDevice(cap))) {
        userError("Specifying a device frame as an IPC buffer is not permitted.");
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (!IS_ALIGNED(vptr, seL4_IPCBufferSizeBits)) {
        userError("IPC Buffer vaddr 0x%x is not aligned.", (int)vptr);
        current_syscall_error.type = seL4_AlignmentError;
        return EXCEPTION_SYSCALL_ERROR;
    }

    return EXCEPTION_NONE;
}

static lookupPGDSlot_ret_t
lookupPGDSlot(vspace_root_t *vspace, vptr_t vptr)
{
    lookupPGDSlot_ret_t ret;

    pgde_t *pgd = PGDE_PTR(vspace);
    word_t pgdIndex = GET_PGD_INDEX(vptr);
    ret.status = EXCEPTION_NONE;
    ret.pgdSlot = pgd + pgdIndex;
    return ret;
}

static lookupPUDSlot_ret_t lookupPUDSlot(vspace_root_t *vspace, vptr_t vptr)
{
    lookupPUDSlot_ret_t ret;

#ifndef CONFIG_START_L1
    lookupPGDSlot_ret_t pgdSlot;

    pgdSlot = lookupPGDSlot(vspace, vptr);

    if (!pgde_ptr_get_present(pgdSlot.pgdSlot)) {
        current_lookup_fault = lookup_fault_missing_capability_new(PGD_INDEX_OFFSET);

        ret.pudSlot = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
    } else {
        pude_t *pud;
        pude_t *pudSlot;
        word_t pudIndex = GET_PUD_INDEX(vptr);
        pud = paddr_to_pptr(pgde_ptr_get_pud_base_address(pgdSlot.pgdSlot));
        pudSlot = pud + pudIndex;

        ret.status = EXCEPTION_NONE;
        ret.pudSlot = pudSlot;
    }
#else
    pude_t *pud = PUDE_PTR(vspace);
    word_t pudIndex = GET_PUD_INDEX(vptr);
    ret.status = EXCEPTION_NONE;
    ret.pudSlot = pud + pudIndex;
#endif

    return ret;
}

static lookupPDSlot_ret_t
lookupPDSlot(vspace_root_t *vspace, vptr_t vptr)
{
    lookupPUDSlot_ret_t pudSlot;
    lookupPDSlot_ret_t ret;

    pudSlot = lookupPUDSlot(vspace, vptr);
    if (pudSlot.status != EXCEPTION_NONE) {
        ret.pdSlot = NULL;
        ret.status = pudSlot.status;
        return ret;
    }
    if (!pude_pude_pd_ptr_get_present(pudSlot.pudSlot)) {
        current_lookup_fault = lookup_fault_missing_capability_new(PUD_INDEX_OFFSET);

        ret.pdSlot = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
        return ret;
    } else {
        pde_t *pd;
        pde_t *pdSlot;
        word_t pdIndex = GET_PD_INDEX(vptr);
        pd = paddr_to_pptr(pude_pude_pd_ptr_get_pd_base_address(pudSlot.pudSlot));
        pdSlot = pd + pdIndex;

        ret.status = EXCEPTION_NONE;
        ret.pdSlot = pdSlot;
        return ret;
    }
}

static lookupPTSlot_ret_t
lookupPTSlot(vspace_root_t *vspace, vptr_t vptr)
{
    lookupPTSlot_ret_t ret;
    lookupPDSlot_ret_t pdSlot;

    pdSlot = lookupPDSlot(vspace, vptr);
    if (pdSlot.status != EXCEPTION_NONE) {
        ret.ptSlot = NULL;
        ret.status = pdSlot.status;
        return ret;
    }
    if (!pde_pde_small_ptr_get_present(pdSlot.pdSlot)) {
        current_lookup_fault = lookup_fault_missing_capability_new(PD_INDEX_OFFSET);

        ret.ptSlot = NULL;
        ret.status = EXCEPTION_LOOKUP_FAULT;
        return ret;
    } else {
        pte_t* pt;
        pte_t* ptSlot;
        word_t ptIndex = GET_PT_INDEX(vptr);
        pt = paddr_to_pptr(pde_pde_small_ptr_get_pt_base_address(pdSlot.pdSlot));
        ptSlot = pt + ptIndex;

        ret.ptSlot = ptSlot;
        ret.status = EXCEPTION_NONE;
        return ret;
    }
}

static lookupFrame_ret_t
lookupFrame(vspace_root_t *vspace, vptr_t vptr)
{
    lookupPUDSlot_ret_t pudSlot;
    lookupFrame_ret_t ret;

    pudSlot = lookupPUDSlot(vspace, vptr);
    if (pudSlot.status != EXCEPTION_NONE) {
        ret.valid = false;
        return ret;
    }

    switch (pude_ptr_get_pude_type(pudSlot.pudSlot)) {
    case pude_pude_1g:
        ret.frameBase = pude_pude_1g_ptr_get_page_base_address(pudSlot.pudSlot);
        ret.frameSize = ARMHugePage;
        ret.valid = true;
        return ret;

    case pude_pude_pd: {
        pde_t *pd = paddr_to_pptr(pude_pude_pd_ptr_get_pd_base_address(pudSlot.pudSlot));
        pde_t *pdSlot = pd + GET_PD_INDEX(vptr);

        if (pde_ptr_get_pde_type(pdSlot) == pde_pde_large) {
            ret.frameBase = pde_pde_large_ptr_get_page_base_address(pdSlot);
            ret.frameSize = ARMLargePage;
            ret.valid = true;
            return ret;
        }

        if (pde_ptr_get_pde_type(pdSlot) == pde_pde_small) {
            pte_t *pt = paddr_to_pptr(pde_pde_small_ptr_get_pt_base_address(pdSlot));
            pte_t *ptSlot = pt + GET_PT_INDEX(vptr);

            if (pte_ptr_get_present(ptSlot)) {
                ret.frameBase = pte_ptr_get_page_base_address(ptSlot);
                ret.frameSize = ARMSmallPage;
                ret.valid = true;
                return ret;
            }
        }
    }
    }

    ret.valid = false;
    return ret;
}

#ifndef CONFIG_ARM_HYPERVISOR_SUPPORT

static pte_t
makeUser3rdLevel(paddr_t paddr, vm_rights_t vm_rights, vm_attributes_t attributes)
{
    bool_t nonexecutable = vm_attributes_get_armExecuteNever(attributes);

    if (vm_attributes_get_armPageCacheable(attributes)) {
        return pte_new(
                   nonexecutable,              /* unprivileged execute never */
                   paddr,
                   1,                          /* not global */
                   1,                          /* access flag */
                   SMP_TERNARY(SMP_SHARE, 0),          /* Inner-shareable if SMP enabled, otherwise unshared */
                   APFromVMRights(vm_rights),
                   NORMAL,
                   0b11                        /* reserved */
               );
    } else {
        return pte_new(
                   nonexecutable,              /* unprivileged execute never */
                   paddr,
                   1,                          /* not global */
                   1,                          /* access flag */
                   0,                          /* Ignored - Outter shareable */
                   APFromVMRights(vm_rights),
                   DEVICE_nGnRnE,
                   0b11                        /* reserved */
               );
    }
}

static pde_t
makeUser2ndLevel(paddr_t paddr, vm_rights_t vm_rights, vm_attributes_t attributes)
{
    bool_t nonexecutable = vm_attributes_get_armExecuteNever(attributes);

    if (vm_attributes_get_armPageCacheable(attributes)) {
        return pde_pde_large_new(
                   nonexecutable,              /* unprivileged execute never */
                   paddr,
                   1,                          /* not global */
                   1,                          /* access flag */
                   SMP_TERNARY(SMP_SHARE, 0),          /* Inner-shareable if SMP enabled, otherwise unshared */
                   APFromVMRights(vm_rights),
                   NORMAL
               );
    } else {
        return pde_pde_large_new(
                   nonexecutable,              /* unprivileged execute never */
                   paddr,
                   1,                          /* not global */
                   1,                          /* access flag */
                   0,                          /* Ignored - Outter shareable */
                   APFromVMRights(vm_rights),
                   DEVICE_nGnRnE
               );
    }
}

#else

static pteS2_t
makeUser3rdLevel(paddr_t paddr, vm_rights_t vm_rights, vm_attributes_t attributes)
{
    /* Stage 2 Translation: XN[1:0] of 0b10 is No Execution @ EL1 or EL0 */
    bool_t nonexecutable = vm_attributes_get_armExecuteNever(attributes) << 1;

    if (vm_attributes_get_armPageCacheable(attributes)) {
        return pteS2_new(
            nonexecutable,
            0,                            /* Not Contiguous */
            0,                            /* Dirty Bit Modifier */
            paddr,
            1,                            /* access flag */
            0,                            /* Non-Shareable */
            APFromVMRights(vm_rights),    /* Read/Write */
            S2_NORMAL,                    /* Inner/Outer Cacheable */
            0b11                          /* 4kB Page */
            );
    } else {
        return pteS2_new(
            nonexecutable,
            0,                            /* Not Contiguous */
            0,                            /* Dirty Bit Modifier */
            paddr,
            1,                            /* access flag */
            0,                            /* Non-Shareable */
            APFromVMRights(vm_rights),    /* Read/Write */
            S2_DEVICE_nGnRnE,             /* Inner/Outer Non-Cacheable */
            0b11                          /* 4kB Page */
            );
    }
}

/* The 2nd Level of the Translation Tables is a 2MB Block.
 *
 * Per the ARMv8 Manual, Stage 2 Translation use the same structure for
 * Blocks and Pages. The only difference is the 2LSBs,
 * which change from 0b11 for pages to 0b01 for blocks.
 */
static pdeS2_t
makeUser2ndLevel(paddr_t paddr, vm_rights_t vm_rights, vm_attributes_t attributes)
{
    /* Stage 2 Translation: XN[1:0] of 0b10 is No Execution @ EL1 or EL0 */
    bool_t nonexecutable = vm_attributes_get_armExecuteNever(attributes) << 1;

    if (vm_attributes_get_armPageCacheable(attributes)) {
        return pdeS2_new(
            nonexecutable,
            0,                            /* Not Contiguous */
            0,                            /* Dirty Bit Modifier */
            paddr,
            1,                            /* access flag */
            0,                            /* Non-Shareable */
            APFromVMRights(vm_rights),    /* Read/Write */
            S2_NORMAL,                    /* Inner/Outer Non-Cacheable */
            0b01                          /* S2 Block */
            );
    } else {
        return pdeS2_new(
            nonexecutable,
            0,                            /* Not Contiguous */
            0,                            /* Dirty Bit Modifier */
            paddr,
            1,                            /* access flag */
            0,                            /* Non-Shareable */
            APFromVMRights(vm_rights),    /* Read/Write */
            S2_DEVICE_nGnRnE,             /* Inner/Outer Non-Cacheable */
            0b01                          /* S2 Block */
            );
    }
}

#endif  /* CONFIG_ARM_HYPERVISOR_SUPPORT */

static pude_t
makeUser1stLevel(paddr_t paddr, vm_rights_t vm_rights, vm_attributes_t attributes)
{
    bool_t nonexecutable = vm_attributes_get_armExecuteNever(attributes);

    if (vm_attributes_get_armPageCacheable(attributes)) {
        return pude_pude_1g_new(
                   nonexecutable,              /* unprivileged execute never */
                   paddr,
                   1,                          /* not global */
                   1,                          /* access flag */
                   SMP_TERNARY(SMP_SHARE, 0),          /* Inner-shareable if SMP enabled, otherwise unshared */
                   APFromVMRights(vm_rights),
                   NORMAL
               );
    } else {
        return pude_pude_1g_new(
                   nonexecutable,              /* unprivileged execute never */
                   paddr,
                   1,                          /* not global */
                   1,                          /* access flag */
                   0,                          /* Ignored - Outter shareable */
                   APFromVMRights(vm_rights),
                   DEVICE_nGnRnE
               );
    }
}

exception_t
handleVMFault(tcb_t *thread, vm_fault_type_t vm_faultType)
{
    switch (vm_faultType) {
    case ARMDataAbort: {
        word_t addr, fault;

        addr = getFAR();
        fault = getDFSR();

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
        addr = s1Translate(addr);
#endif

        current_fault = seL4_Fault_VMFault_new(addr, fault, false);
        return EXCEPTION_FAULT;
    }

    case ARMPrefetchAbort: {
        word_t pc, fault;

        pc = getRestartPC(thread);

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
        pc = s1Translate(pc);
#endif

        fault = getIFSR();
        current_fault = seL4_Fault_VMFault_new(pc, fault, true);
        return EXCEPTION_FAULT;
    }

    default:
        fail("Invalid VM fault type");
    }
}

bool_t CONST
isVTableRoot(cap_t cap)
{
    return cap_get_capType(cap) == vspace_sl.top_cap;
}

bool_t CONST
isValidNativeRoot(cap_t cap)
{
    return isVTableRoot(cap) &&
           vspace_sl.top_cap_mapped(cap);
}

bool_t CONST
isValidVTableRoot(cap_t cap)
{
    return isValidNativeRoot(cap);
}

bool_t CONST
isIOSpaceFrameCap(cap_t cap)
{
#ifdef CONFIG_ARM_SMMU
    return cap_frame_cap_get_capFSize(cap) == ARMSmallPage && cap_frame_cap_get_capFIsIOSpace(cap);
#else
    return false;
#endif
}

void
setVMRoot(tcb_t *tcb)
{
    cap_t threadRoot;
    asid_t asid;
    void *top;

    findVSpaceForASID_ret_t find_ret;
    threadRoot = TCB_PTR_CTE_PTR(tcb, tcbVTable)->cap;
    if (!isValidNativeRoot(threadRoot)) {
        setCurrentUserVSpaceRoot(ttbr_new(0, pptr_to_paddr(armKSGlobalUserPGD)));
        return;
    }

    top = PGD_PTR(vspace_sl.top_base_ptr(threadRoot));
    asid = vspace_sl.top_cap_asid(threadRoot);
    find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE || find_ret.vspace_root != (vspace_root_t *)top)) {
        setCurrentUserVSpaceRoot(ttbr_new(0, pptr_to_paddr(armKSGlobalUserPGD)));
        return;
    }

    armv_contextSwitch((vspace_root_t *)top, asid);
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        vcpu_switch(tcb->tcbArch.tcbVCPU);
    }
}

static bool_t
setVMRootForFlush(vspace_root_t *vspace, asid_t asid)
{
    cap_t threadRoot;

    threadRoot = TCB_PTR_CTE_PTR(NODE_STATE(ksCurThread), tcbVTable)->cap;

    if (cap_get_capType(threadRoot) == vspace_sl.top_cap &&
            vspace_sl.top_cap_mapped(threadRoot) &&
        (vspace_root_t *)vspace_sl.top_base_ptr(threadRoot) == vspace) {
        return false;
    }

    armv_contextSwitch(vspace, asid);
    return true;
}

pgde_t *pageUpperDirectoryMapped(asid_t asid, vptr_t vaddr, pude_t* pud)
{
    findVSpaceForASID_ret_t find_ret;
    lookupPGDSlot_ret_t lu_ret;

    find_ret = findVSpaceForASID(asid);
    if (find_ret.status != EXCEPTION_NONE) {
        return NULL;
    }

    lu_ret = lookupPGDSlot(find_ret.vspace_root, vaddr);
    if (pgde_ptr_get_present(lu_ret.pgdSlot) &&
            (pgde_ptr_get_pud_base_address(lu_ret.pgdSlot) == pptr_to_paddr(pud))) {
        return lu_ret.pgdSlot;
    }

    return NULL;
}

pude_t *pageDirectoryMapped(asid_t asid, vptr_t vaddr, pde_t* pd)
{
    findVSpaceForASID_ret_t find_ret;
    lookupPUDSlot_ret_t lu_ret;

    find_ret = findVSpaceForASID(asid);
    if (find_ret.status != EXCEPTION_NONE) {
        return NULL;
    }

    lu_ret = lookupPUDSlot(find_ret.vspace_root, vaddr);
    if (lu_ret.status != EXCEPTION_NONE) {
        return NULL;
    }

    if (pude_pude_pd_ptr_get_present(lu_ret.pudSlot) &&
            (pude_pude_pd_ptr_get_pd_base_address(lu_ret.pudSlot) == pptr_to_paddr(pd))) {
        return lu_ret.pudSlot;
    }

    return NULL;
}

pde_t *pageTableMapped(asid_t asid, vptr_t vaddr, pte_t* pt)
{
    findVSpaceForASID_ret_t find_ret;
    lookupPDSlot_ret_t lu_ret;

    find_ret = findVSpaceForASID(asid);
    if (find_ret.status != EXCEPTION_NONE) {
        return NULL;
    }

    lu_ret = lookupPDSlot(find_ret.vspace_root, vaddr);
    if (lu_ret.status != EXCEPTION_NONE) {
        return NULL;
    }

    if (pde_pde_small_ptr_get_present(lu_ret.pdSlot) &&
            (pde_pde_small_ptr_get_pt_base_address(lu_ret.pdSlot) == pptr_to_paddr(pt))) {
        return lu_ret.pdSlot;
    }

    return NULL;
}

void unmapPageUpperDirectory(asid_t asid, vptr_t vaddr, pude_t* pud)
{
    pgde_t *pgdSlot;

    pgdSlot = pageUpperDirectoryMapped(asid, vaddr, pud);
    if (likely(pgdSlot != NULL)) {
        *pgdSlot = pgde_invalid_new();

        cleanByVA_PoU((vptr_t)pgdSlot, pptr_to_paddr(pgdSlot));
        invalidateTranslationASID(asid);
    }
}

void unmapPageDirectory(asid_t asid, vptr_t vaddr, pde_t* pd)
{
    pude_t *pudSlot;

    pudSlot = pageDirectoryMapped(asid, vaddr, pd);
    if (likely(pudSlot != NULL)) {
        *pudSlot = pude_invalid_new();

        cleanByVA_PoU((vptr_t)pudSlot, pptr_to_paddr(pudSlot));
        invalidateTranslationASID(asid);
    }
}

void unmapPageTable(asid_t asid, vptr_t vaddr, pte_t *pt)
{
    pde_t *pdSlot;

    pdSlot = pageTableMapped(asid, vaddr, pt);
    if (likely(pdSlot != NULL)) {
        *pdSlot = pde_invalid_new();

        cleanByVA_PoU((vptr_t)pdSlot, pptr_to_paddr(pdSlot));
        invalidateTranslationASID(asid);
    }
}

void unmapPage(vm_page_size_t page_size, asid_t asid, vptr_t vptr, pptr_t pptr)
{
    paddr_t addr;
    findVSpaceForASID_ret_t find_ret;

    addr = pptr_to_paddr((void *)pptr);
    find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE)) {
        return;
    }

    switch (page_size) {
    case ARMSmallPage: {
        lookupPTSlot_ret_t lu_ret;

        lu_ret = lookupPTSlot(find_ret.vspace_root, vptr);
        if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
            return;
        }

        if (pte_ptr_get_present(lu_ret.ptSlot) &&
                pte_ptr_get_page_base_address(lu_ret.ptSlot) == addr) {
            *(lu_ret.ptSlot) = pte_invalid_new();

            cleanByVA_PoU((vptr_t)lu_ret.ptSlot, pptr_to_paddr(lu_ret.ptSlot));
        }
        break;
    }

    case ARMLargePage: {
        lookupPDSlot_ret_t lu_ret;

        lu_ret = lookupPDSlot(find_ret.vspace_root, vptr);
        if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
            return;
        }

        if (pde_pde_large_ptr_get_present(lu_ret.pdSlot) &&
                pde_pde_large_ptr_get_page_base_address(lu_ret.pdSlot) == addr) {
            *(lu_ret.pdSlot) = pde_invalid_new();

            cleanByVA_PoU((vptr_t)lu_ret.pdSlot, pptr_to_paddr(lu_ret.pdSlot));
        }
        break;
    }

    case ARMHugePage: {
        lookupPUDSlot_ret_t lu_ret;

        lu_ret = lookupPUDSlot(find_ret.vspace_root, vptr);
        if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
            return;
        }

        if (pude_pude_1g_ptr_get_present(lu_ret.pudSlot) &&
                pude_pude_1g_ptr_get_page_base_address(lu_ret.pudSlot) == addr) {
            *(lu_ret.pudSlot) = pude_invalid_new();

            cleanByVA_PoU((vptr_t)lu_ret.pudSlot, pptr_to_paddr(lu_ret.pudSlot));
        }
        break;
    }

    default:
        fail("Invalid ARM page type");
    }

#ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
    assert(asid < BIT(16));
    invalidateTranslationSingle((asid << 48) | vptr >> seL4_PageBits);
#else
    invalidateTranslationSingle(vptr >> seL4_PageBits);
#endif
}

void
deleteASID(asid_t asid, vspace_root_t *vspace)
{
    asid_pool_t *poolPtr;

    poolPtr = armKSASIDTable[asid >> asidLowBits];

    if (poolPtr != NULL && poolPtr->array[asid & MASK(asidLowBits)] == vspace) {
        invalidateTranslationASID(asid);
        poolPtr->array[asid & MASK(asidLowBits)] = NULL;
        setVMRoot(NODE_STATE(ksCurThread));
    }
}

void
deleteASIDPool(asid_t asid_base, asid_pool_t* pool)
{
    word_t offset;

    assert((asid_base & MASK(asidLowBits)) == 0);

    if (armKSASIDTable[asid_base >> asidLowBits] == pool) {
        for (offset = 0; offset < BIT(asidLowBits); offset++) {
            if (pool->array[offset]) {
                invalidateTranslationASID(asid_base + offset);
            }
        }
        armKSASIDTable[asid_base >> asidLowBits] = NULL;
        setVMRoot(NODE_STATE(ksCurThread));
    }
}

static void
doFlush(int invLabel, vptr_t start, vptr_t end, paddr_t pstart)
{
    switch (invLabel) {
    case ARMPageGlobalDirectoryClean_Data:
    case ARMPageClean_Data:
        cleanCacheRange_RAM(start, end, pstart);
        break;

    case ARMPageGlobalDirectoryInvalidate_Data:
    case ARMPageInvalidate_Data:
        invalidateCacheRange_RAM(start, end, pstart);
        break;

    case ARMPageGlobalDirectoryCleanInvalidate_Data:
    case ARMPageCleanInvalidate_Data:
        cleanInvalidateCacheRange_RAM(start, end, pstart);
        break;

    case ARMPageGlobalDirectoryUnify_Instruction:
    case ARMPageUnify_Instruction:
        /* First clean data lines to point of unification... */
        cleanCacheRange_PoU(start, end, pstart);
        /* Ensure it's been written. */
        dsb();
        /* ...then invalidate the corresponding instruction lines
           to point of unification... */
        invalidateCacheRange_I(start, end, pstart);
        /* ... and ensure new instructions come from fresh cache lines. */
        isb();
        break;
    default:
        fail("Invalid operation, shouldn't get here.\n");
    }
}

/* ================= INVOCATION HANDLING STARTS HERE ================== */

static exception_t
performPageGlobalDirectoryFlush(int invLabel, pgde_t *pgd, asid_t asid,
                                vptr_t start, vptr_t end, paddr_t pstart)
{
    bool_t root_switched;

    /* Flush if given a non zero range */
    if (start < end) {
        root_switched = setVMRootForFlush(pgd, asid);
        doFlush(invLabel, start, end, pstart);
        if (root_switched) {
            setVMRoot(NODE_STATE(ksCurThread));
        }
    }
    return EXCEPTION_NONE;
}

static exception_t
performUpperPageDirectoryInvocationMap(cap_t cap, cte_t *ctSlot, pgde_t pgde, pgde_t *pgdSlot)
{
    ctSlot->cap = cap;
    *pgdSlot = pgde;
    cleanByVA_PoU((vptr_t)pgdSlot, pptr_to_paddr(pgdSlot));

    return EXCEPTION_NONE;
}

static exception_t
performUpperPageDirectoryInvocationUnmap(cap_t cap, cte_t *ctSlot)
{
    if (cap_page_upper_directory_cap_get_capPUDIsMapped(cap)) {
        pude_t *pud = PUD_PTR(cap_page_upper_directory_cap_get_capPUDBasePtr(cap));
        unmapPageUpperDirectory(cap_page_upper_directory_cap_get_capPUDMappedASID(cap),
                                cap_page_upper_directory_cap_get_capPUDMappedAddress(cap), pud);
        clearMemory((void *)pud, cap_get_capSizeBits(cap));
    }

    cap_page_upper_directory_cap_ptr_set_capPUDIsMapped(&(ctSlot->cap), 0);
    return EXCEPTION_NONE;
}

static exception_t
performPageDirectoryInvocationMap(cap_t cap, cte_t *ctSlot, pude_t pude, pude_t *pudSlot)
{
    ctSlot->cap = cap;
    *pudSlot = pude;
    cleanByVA_PoU((vptr_t)pudSlot, pptr_to_paddr(pudSlot));

    return EXCEPTION_NONE;
}

static exception_t
performPageDirectoryInvocationUnmap(cap_t cap, cte_t *ctSlot)
{
    if (cap_page_directory_cap_get_capPDIsMapped(cap)) {
        pde_t *pd = PD_PTR(cap_page_directory_cap_get_capPDBasePtr(cap));
        unmapPageDirectory(cap_page_directory_cap_get_capPDMappedASID(cap),
                           cap_page_directory_cap_get_capPDMappedAddress(cap), pd);
        clearMemory((void *)pd, cap_get_capSizeBits(cap));
    }

    cap_page_directory_cap_ptr_set_capPDIsMapped(&(ctSlot->cap), 0);
    return EXCEPTION_NONE;
}

static exception_t
performPageTableInvocationMap(cap_t cap, cte_t *ctSlot, pde_t pde, pde_t *pdSlot)
{
    ctSlot->cap = cap;
    *pdSlot = pde;
    cleanByVA_PoU((vptr_t)pdSlot, pptr_to_paddr(pdSlot));

    return EXCEPTION_NONE;
}

static exception_t
performPageTableInvocationUnmap(cap_t cap, cte_t *ctSlot)
{
    if (cap_page_table_cap_get_capPTIsMapped(cap)) {
        pte_t *pt = PT_PTR(cap_page_table_cap_get_capPTBasePtr(cap));
        unmapPageTable(cap_page_table_cap_get_capPTMappedASID(cap),
                       cap_page_table_cap_get_capPTMappedAddress(cap), pt);
        clearMemory((void *)pt, cap_get_capSizeBits(cap));
    }

    cap_page_table_cap_ptr_set_capPTIsMapped(&(ctSlot->cap), 0);
    return EXCEPTION_NONE;
}

static exception_t
performHugePageInvocationMap(asid_t asid, cap_t cap, cte_t *ctSlot,
                             pude_t pude, pude_t *pudSlot)
{
    bool_t tlbflush_required = pude_pude_1g_ptr_get_present(pudSlot);

    ctSlot->cap = cap;
    *pudSlot = pude;

    cleanByVA_PoU((vptr_t)pudSlot, pptr_to_paddr(pudSlot));
    if (unlikely(tlbflush_required)) {
        assert(asid < BIT(16));
        invalidateTranslationSingle((asid << 48) |
                                    cap_frame_cap_get_capFMappedAddress(cap) >> seL4_PageBits);
    }

    return EXCEPTION_NONE;
}

static exception_t
performLargePageInvocationMap(asid_t asid, cap_t cap, cte_t *ctSlot,
                              dir_t pde, dir_t *pdSlot)
{
    bool_t tlbflush_required = pde_pde_large_ptr_get_present((pde_t *)pdSlot);

    ctSlot->cap = cap;
    *pdSlot = pde;

    cleanByVA_PoU((vptr_t)pdSlot, pptr_to_paddr(pdSlot));
    if (unlikely(tlbflush_required)) {
        assert(asid < BIT(16));
        invalidateTranslationSingle((asid << 48) |
                                    cap_frame_cap_get_capFMappedAddress(cap) >> seL4_PageBits);
    }

    return EXCEPTION_NONE;
}

static exception_t
performSmallPageInvocationMap(asid_t asid, cap_t cap, cte_t *ctSlot,
                              table_t pte, table_t *ptSlot)
{
    bool_t tlbflush_required = pte_ptr_get_present((pte_t *)ptSlot);

    ctSlot->cap = cap;
    *ptSlot = pte;

    cleanByVA_PoU((vptr_t)ptSlot, pptr_to_paddr(ptSlot));
    if (unlikely(tlbflush_required)) {
#ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
        assert(asid < BIT(16));
        invalidateTranslationSingle((asid << 48) |
                                    cap_frame_cap_get_capFMappedAddress(cap) >> seL4_PageBits);
#else
        invalidateTranslationSingle(cap_frame_cap_get_capFMappedAddress(cap) >> seL4_PageBits);
#endif
    }

    return EXCEPTION_NONE;
}

static exception_t
performPageInvocationUnmap(cap_t cap, cte_t *ctSlot)
{
    if (cap_frame_cap_get_capFMappedASID(cap) != 0) {
        unmapPage(cap_frame_cap_get_capFSize(cap),
                  cap_frame_cap_get_capFMappedASID(cap),
                  cap_frame_cap_get_capFMappedAddress(cap),
                  cap_frame_cap_get_capFBasePtr(cap));
    }

    cap_frame_cap_ptr_set_capFMappedASID(&ctSlot->cap, asidInvalid);
    cap_frame_cap_ptr_set_capFMappedAddress(&ctSlot->cap, 0);
    return EXCEPTION_NONE;
}

static exception_t
performPageFlush(int invLabel, pgde_t *pgd, asid_t asid,
                 vptr_t start, vptr_t end, paddr_t pstart)
{
    bool_t root_switched;

    if (start < end) {
        root_switched = setVMRootForFlush(pgd, asid);
        doFlush(invLabel, start, end, pstart);
        if (root_switched) {
            setVMRoot(NODE_STATE(ksCurThread));
        }
    }
    return EXCEPTION_NONE;
}

static exception_t
performPageGetAddress(pptr_t base_ptr)
{
    paddr_t base = pptr_to_paddr((void *)base_ptr);

    setRegister(NODE_STATE(ksCurThread), msgRegisters[0], base);
    setRegister(NODE_STATE(ksCurThread), msgInfoRegister,
                wordFromMessageInfo(seL4_MessageInfo_new(0, 0, 0, 1)));

    return EXCEPTION_NONE;
}

static exception_t
performASIDPoolInvocation(asid_t asid, asid_pool_t *poolPtr, cte_t *vspaceCapSlot)
{
    vspace_sl.top_cap_ptr_map(&vspaceCapSlot->cap, asid);
    vspace_sl.top_cap_ptr_mapped(&vspaceCapSlot->cap, 1);

    poolPtr->array[asid & MASK(asidLowBits)] =
        PGD_PTR(vspace_sl.top_base_ptr(vspaceCapSlot->cap));

    return EXCEPTION_NONE;
}

static exception_t
performASIDControlInvocation(void *frame, cte_t *slot,
                             cte_t *parent, asid_t asid_base)
{
    cap_untyped_cap_ptr_set_capFreeIndex(&(parent->cap),
                                         MAX_FREE_INDEX(cap_untyped_cap_get_capBlockSize(parent->cap)));

    memzero(frame, BIT(pageBitsForSize(ARMSmallPage)));

    cteInsert(
        cap_asid_pool_cap_new(
            asid_base,         /* capASIDBase  */
            WORD_REF(frame)    /* capASIDPool  */
        ), parent, slot);

    assert((asid_base & MASK(asidLowBits)) == 0);
    armKSASIDTable[asid_base >> asidLowBits] = (asid_pool_t *)frame;

    return EXCEPTION_NONE;
}

static exception_t
decodeARMPageGlobalDirectoryInvocation(word_t invLabel, unsigned int length,
                                       cte_t *cte, cap_t cap, extra_caps_t extraCaps,
                                       word_t *buffer)
{
    vptr_t start, end;
    paddr_t pstart;
    asid_t asid;
    pgde_t *pgd;
    lookupFrame_ret_t resolve_ret;
    findVSpaceForASID_ret_t find_ret;

    switch (invLabel) {
    case ARMPageGlobalDirectoryClean_Data:
    case ARMPageGlobalDirectoryInvalidate_Data:
    case ARMPageGlobalDirectoryCleanInvalidate_Data:
    case ARMPageGlobalDirectoryUnify_Instruction:

        if (length < 2) {
            userError("PGD Flush: Truncated message.");
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        start = getSyscallArg(0, buffer);
        end =   getSyscallArg(1, buffer);

        /* Check sanity of arguments */
        if (end <= start) {
            userError("PGD Flush: Invalid range.");
            current_syscall_error.type = seL4_InvalidArgument;
            current_syscall_error.invalidArgumentNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Don't let applications flush kernel regions. */
        if (end > USER_TOP) {
            userError("PGD Flush: Exceed the user addressable region.");
            current_syscall_error.type = seL4_IllegalOperation;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(!isValidNativeRoot(cap))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Make sure that the supplied pgd is ok */
        pgd = PGDE_PTR(cap_page_global_directory_cap_get_capPGDBasePtr(cap));
        asid = cap_page_global_directory_cap_get_capPGDMappedASID(cap);

        find_ret = findVSpaceForASID(asid);
        if (unlikely(find_ret.status != EXCEPTION_NONE)) {
            userError("PGD Flush: No PGD for ASID");
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(find_ret.vspace_root != pgd)) {
            userError("PGD Flush: Invalid PGD Cap");
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Look up the frame containing 'start'. */
        resolve_ret = lookupFrame(pgd, start);

        if (!resolve_ret.valid) {
            /* Fail silently, as there can't be any stale cached data (for the
             * given address space), and getting a syscall error because the
             * relevant page is non-resident would be 'astonishing'. */
            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return EXCEPTION_NONE;
        }

        /* Refuse to cross a page boundary. */
        if (PAGE_BASE(start, resolve_ret.frameSize) != PAGE_BASE(end - 1, resolve_ret.frameSize)) {
            current_syscall_error.type = seL4_RangeError;
            current_syscall_error.rangeErrorMin = start;
            current_syscall_error.rangeErrorMax = PAGE_BASE(start, resolve_ret.frameSize) +
                                                  MASK(pageBitsForSize(resolve_ret.frameSize));
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Calculate the physical start address. */
        pstart = resolve_ret.frameBase + PAGE_OFFSET(start, resolve_ret.frameSize);

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageGlobalDirectoryFlush(invLabel, pgd, asid, start, end - 1, pstart);

    default:
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }
}

static exception_t
decodeARMPageUpperDirectoryInvocation(word_t invLabel, unsigned int length,
                                      cte_t *cte, cap_t cap, extra_caps_t extraCaps,
                                      word_t *buffer)
{
    cap_t pgdCap;
    pgde_t *pgd;
    pgde_t pgde;
    asid_t asid;
    vptr_t vaddr;
    lookupPGDSlot_ret_t pgdSlot;
    findVSpaceForASID_ret_t find_ret;

#ifdef CONFIG_START_L1

    if (invLabel == ARMPageGlobalDirectoryClean_Data ||
        invLabel == ARMPageGlobalDirectoryInvalidate_Data ||
        invLabel == ARMPageGlobalDirectoryCleanInvalidate_Data ||
        invLabel == ARMPageGlobalDirectoryUnify_Instruction) {

        vptr_t start, end;
        paddr_t pstart;
        word_t size;
        lookupFrame_ret_t resolve_ret;

        if (length < 2) {
            userError("PUD Flush: Truncated message.");
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* This gets the IPAs. We need to find the physical frames */
        start = getSyscallArg(0, buffer);
        end =   getSyscallArg(1, buffer);
        size =  end - start;

        /* Check sanity of arguments */
        if (end <= start) {
            userError("PUD Flush: Invalid range.");
            current_syscall_error.type = seL4_InvalidArgument;
            current_syscall_error.invalidArgumentNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Don't let applications flush kernel regions. */
        if (end > USER_TOP) {
            userError("PUD Flush: Exceed the user addressable region.");
            current_syscall_error.type = seL4_IllegalOperation;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(!isValidNativeRoot(cap))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Make sure that the supplied pgd is ok */
        pgd = PGDE_PTR(cap_page_upper_directory_cap_get_capPUDBasePtr(cap));
        asid = cap_page_upper_directory_cap_get_capPUDMappedASID(cap);

        find_ret = findVSpaceForASID(asid);
        if (unlikely(find_ret.status != EXCEPTION_NONE)) {
            userError("PUD Flush: No PUD for ASID");
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(find_ret.vspace_root != pgd)) {
            userError("PUD Flush: Invalid PUD Cap");
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Look up the frame containing 'start' IPA */
        resolve_ret = lookupFrame(pgd, (vptr_t)paddr_to_pptr(start));

        start = (vptr_t)paddr_to_pptr(resolve_ret.frameBase);
        end = start + size;

        if (!resolve_ret.valid) {
            /* Fail silently, as there can't be any stale cached data (for the
             * given address space), and getting a syscall error because the
             * relevant page is non-resident would be 'astonishing'. */
            setThreadState(ksCurThread, ThreadState_Restart);
            return EXCEPTION_NONE;
        }

        /* Refuse to cross a page boundary. */
        if (PAGE_BASE(start, resolve_ret.frameSize) != PAGE_BASE(end - 1, resolve_ret.frameSize)) {
            current_syscall_error.type = seL4_RangeError;
            current_syscall_error.rangeErrorMin = start;
            current_syscall_error.rangeErrorMax = PAGE_BASE(start, resolve_ret.frameSize) +
                MASK(pageBitsForSize(resolve_ret.frameSize));
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Calculate the physical start address. */
        pstart = resolve_ret.frameBase + PAGE_OFFSET(start, resolve_ret.frameSize);

        setThreadState(ksCurThread, ThreadState_Restart);
        return performPageGlobalDirectoryFlush(invLabel, pgd, asid, start, end - 1, pstart);
    }

#endif

    if (invLabel == ARMPageUpperDirectoryUnmap) {
        if (unlikely(!isFinalCapability(cte))) {
            current_syscall_error.type = seL4_RevokeFirst;
            return EXCEPTION_SYSCALL_ERROR;
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performUpperPageDirectoryInvocationUnmap(cap, cte);
    }

    if (unlikely(invLabel != ARMPageUpperDirectoryMap)) {
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(length < 2 || extraCaps.excaprefs[0] == NULL)) {
        current_syscall_error.type = seL4_TruncatedMessage;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(cap_page_upper_directory_cap_get_capPUDIsMapped(cap))) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    vaddr = getSyscallArg(0, buffer) & (~MASK(PGD_INDEX_OFFSET));
    pgdCap = extraCaps.excaprefs[0]->cap;

    if (unlikely(!isValidNativeRoot(pgdCap))) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pgd = PGDE_PTR(vspace_sl.top_base_ptr(pgdCap));
    asid = vspace_sl.top_cap_asid(pgdCap);

    if (unlikely(vaddr > USER_TOP)) {
        current_syscall_error.type = seL4_InvalidArgument;
        current_syscall_error.invalidArgumentNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE)) {
        current_syscall_error.type = seL4_FailedLookup;
        current_syscall_error.failedLookupWasSource = false;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(find_ret.vspace_root != pgd)) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pgdSlot = lookupPGDSlot(pgd, vaddr);

    if (unlikely(pgde_ptr_get_present(pgdSlot.pgdSlot))) {
        current_syscall_error.type = seL4_DeleteFirst;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pgde = pgde_new(
               pptr_to_paddr(PUDE_PTR(cap_page_upper_directory_cap_get_capPUDBasePtr(cap))),
               0b11  /* reserved */
           );

    cap_page_upper_directory_cap_ptr_set_capPUDIsMapped(&cap, 1);
    cap_page_upper_directory_cap_ptr_set_capPUDMappedASID(&cap, asid);
    cap_page_upper_directory_cap_ptr_set_capPUDMappedAddress(&cap, vaddr);

    setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
    return performUpperPageDirectoryInvocationMap(cap, cte, pgde, pgdSlot.pgdSlot);
}

static exception_t
decodeARMPageDirectoryInvocation(word_t invLabel, unsigned int length,
                                 cte_t *cte, cap_t cap, extra_caps_t extraCaps,
                                 word_t *buffer)
{
    cap_t pgdCap;
    pgde_t *pgd;
    pude_t pude;
    asid_t asid;
    vptr_t vaddr;
    lookupPUDSlot_ret_t pudSlot;
    findVSpaceForASID_ret_t find_ret;

    if (invLabel == ARMPageDirectoryUnmap) {
        if (unlikely(!isFinalCapability(cte))) {
            current_syscall_error.type = seL4_RevokeFirst;
            return EXCEPTION_SYSCALL_ERROR;
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageDirectoryInvocationUnmap(cap, cte);
    }

    if (unlikely(invLabel != ARMPageDirectoryMap)) {
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(length < 2 || extraCaps.excaprefs[0] == NULL)) {
        current_syscall_error.type = seL4_TruncatedMessage;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(cap_page_directory_cap_get_capPDIsMapped(cap))) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    vaddr = getSyscallArg(0, buffer) & (~MASK(PUD_INDEX_OFFSET));
    pgdCap = extraCaps.excaprefs[0]->cap;

    if (unlikely(!isValidNativeRoot(pgdCap))) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pgd = PGDE_PTR(vspace_sl.top_base_ptr(pgdCap));
    asid = vspace_sl.top_cap_asid(pgdCap);

    if (unlikely(vaddr > USER_TOP)) {
        current_syscall_error.type = seL4_InvalidArgument;
        current_syscall_error.invalidArgumentNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE)) {
        current_syscall_error.type = seL4_FailedLookup;
        current_syscall_error.failedLookupWasSource = false;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(find_ret.vspace_root != pgd)) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pudSlot = lookupPUDSlot(pgd, vaddr);

    if (pudSlot.status != EXCEPTION_NONE) {
        current_syscall_error.type = seL4_FailedLookup;
        current_syscall_error.failedLookupWasSource = false;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(pude_pude_pd_ptr_get_present(pudSlot.pudSlot) ||
                 pude_pude_1g_ptr_get_present(pudSlot.pudSlot))) {
        current_syscall_error.type = seL4_DeleteFirst;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pude = pude_pude_pd_new(pptr_to_paddr(PDE_PTR(cap_page_directory_cap_get_capPDBasePtr(cap))));

    cap_page_directory_cap_ptr_set_capPDIsMapped(&cap, 1);
    cap_page_directory_cap_ptr_set_capPDMappedASID(&cap, asid);
    cap_page_directory_cap_ptr_set_capPDMappedAddress(&cap, vaddr);

    setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
    return performPageDirectoryInvocationMap(cap, cte, pude, pudSlot.pudSlot);
}

static exception_t
decodeARMPageTableInvocation(word_t invLabel, unsigned int length,
                             cte_t *cte, cap_t cap, extra_caps_t extraCaps,
                             word_t *buffer)
{
    cap_t pgdCap;
    pgde_t *pgd;
    pde_t pde;
    asid_t asid;
    vptr_t vaddr;
    lookupPDSlot_ret_t pdSlot;
    findVSpaceForASID_ret_t find_ret;

    if (invLabel == ARMPageTableUnmap) {
        if (unlikely(!isFinalCapability(cte))) {
            current_syscall_error.type = seL4_RevokeFirst;
            return EXCEPTION_SYSCALL_ERROR;
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageTableInvocationUnmap(cap, cte);
    }

    if (unlikely(invLabel != ARMPageTableMap)) {
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(length < 2 || extraCaps.excaprefs[0] == NULL)) {
        current_syscall_error.type = seL4_TruncatedMessage;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(cap_page_table_cap_get_capPTIsMapped(cap))) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    vaddr = getSyscallArg(0, buffer) & (~MASK(PD_INDEX_OFFSET));
    pgdCap = extraCaps.excaprefs[0]->cap;

    if (unlikely(!isValidNativeRoot(pgdCap))) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pgd = PGDE_PTR(vspace_sl.top_base_ptr(pgdCap));
    asid = vspace_sl.top_cap_asid(pgdCap);

    if (unlikely(vaddr > USER_TOP)) {
        current_syscall_error.type = seL4_InvalidArgument;
        current_syscall_error.invalidArgumentNumber = 0;
        return EXCEPTION_SYSCALL_ERROR;
    }

    find_ret = findVSpaceForASID(asid);
    if (unlikely(find_ret.status != EXCEPTION_NONE)) {
        current_syscall_error.type = seL4_FailedLookup;
        current_syscall_error.failedLookupWasSource = false;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(find_ret.vspace_root != pgd)) {
        current_syscall_error.type = seL4_InvalidCapability;
        current_syscall_error.invalidCapNumber = 1;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pdSlot = lookupPDSlot(pgd, vaddr);

    if (pdSlot.status != EXCEPTION_NONE) {
        current_syscall_error.type = seL4_FailedLookup;
        current_syscall_error.failedLookupWasSource = false;
        return EXCEPTION_SYSCALL_ERROR;
    }

    if (unlikely(pde_pde_small_ptr_get_present(pdSlot.pdSlot) ||
                 pde_pde_large_ptr_get_present(pdSlot.pdSlot))) {
        current_syscall_error.type = seL4_DeleteFirst;
        return EXCEPTION_SYSCALL_ERROR;
    }

    pde = pde_pde_small_new(pptr_to_paddr(PTE_PTR(cap_page_table_cap_get_capPTBasePtr(cap))));

    cap_page_table_cap_ptr_set_capPTIsMapped(&cap, 1);
    cap_page_table_cap_ptr_set_capPTMappedASID(&cap, asid);
    cap_page_table_cap_ptr_set_capPTMappedAddress(&cap, vaddr);

    setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
    return performPageTableInvocationMap(cap, cte, pde, pdSlot.pdSlot);
}

static exception_t
decodeARMFrameInvocation(word_t invLabel, unsigned int length,
                         cte_t *cte, cap_t cap, extra_caps_t extraCaps,
                         word_t *buffer)
{
    switch (invLabel) {
    case ARMPageMap: {
        vptr_t vaddr;
        paddr_t base;
        cap_t pgdCap;
        pgde_t *pgd;
        asid_t asid;
        vm_rights_t vmRights;
        vm_page_size_t frameSize;
        vm_attributes_t attributes;
        findVSpaceForASID_ret_t find_ret;

        if (unlikely(length < 3 || extraCaps.excaprefs[0] == NULL)) {
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(cap_frame_cap_get_capFMappedASID(cap) != 0)) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        vaddr = getSyscallArg(0, buffer);
        attributes = vmAttributesFromWord(getSyscallArg(2, buffer));
        pgdCap = extraCaps.excaprefs[0]->cap;

        frameSize = cap_frame_cap_get_capFSize(cap);
        vmRights = maskVMRights(cap_frame_cap_get_capFVMRights(cap),
                                rightsFromWord(getSyscallArg(1, buffer)));

        if (unlikely(!isValidNativeRoot(pgdCap))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        pgd = PGDE_PTR(vspace_sl.top_base_ptr(pgdCap));
        asid = vspace_sl.top_cap_asid(pgdCap);

        find_ret = findVSpaceForASID(asid);
        if (unlikely(find_ret.status != EXCEPTION_NONE)) {
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(find_ret.vspace_root != pgd)) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(!IS_PAGE_ALIGNED(vaddr, frameSize))) {
            current_syscall_error.type = seL4_AlignmentError;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(vaddr + BIT(pageBitsForSize(frameSize)) - 1 > USER_TOP)) {
            current_syscall_error.type = seL4_InvalidArgument;
            current_syscall_error.invalidArgumentNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        cap = cap_frame_cap_set_capFMappedASID(cap, asid);
        cap = cap_frame_cap_set_capFMappedAddress(cap, vaddr);

        base = pptr_to_paddr((void *)cap_frame_cap_get_capFBasePtr(cap));

        if (frameSize == ARMSmallPage) {
            lookupPTSlot_ret_t lu_ret = lookupPTSlot(pgd, vaddr);

            if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
                current_syscall_error.type = seL4_FailedLookup;
                current_syscall_error.failedLookupWasSource = false;
                return EXCEPTION_SYSCALL_ERROR;
            }

            if (pte_ptr_get_present(lu_ret.ptSlot)) {
                current_syscall_error.type = seL4_DeleteFirst;
                return EXCEPTION_SYSCALL_ERROR;
            }

            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return performSmallPageInvocationMap(asid, cap, cte,
                                                 makeUser3rdLevel(base, vmRights, attributes), (table_t *)lu_ret.ptSlot);

        } else if (frameSize == ARMLargePage) {
            lookupPDSlot_ret_t lu_ret = lookupPDSlot(pgd, vaddr);

            if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
                current_syscall_error.type = seL4_FailedLookup;
                current_syscall_error.failedLookupWasSource = false;
                return EXCEPTION_SYSCALL_ERROR;
            }

            if (pde_pde_small_ptr_get_present(lu_ret.pdSlot) ||
                    pde_pde_large_ptr_get_present(lu_ret.pdSlot)) {
                current_syscall_error.type = seL4_DeleteFirst;
                return EXCEPTION_SYSCALL_ERROR;
            }

            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return performLargePageInvocationMap(asid, cap, cte,
                                                 makeUser2ndLevel(base, vmRights, attributes), (dir_t *)lu_ret.pdSlot);

        } else {
            lookupPUDSlot_ret_t lu_ret = lookupPUDSlot(pgd, vaddr);

            if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
                current_syscall_error.type = seL4_FailedLookup;
                current_syscall_error.failedLookupWasSource = false;
                return EXCEPTION_SYSCALL_ERROR;
            }

            if (pude_pude_pd_ptr_get_present(lu_ret.pudSlot) ||
                    pude_pude_1g_ptr_get_present(lu_ret.pudSlot)) {
                current_syscall_error.type = seL4_DeleteFirst;
                return EXCEPTION_SYSCALL_ERROR;
            }

            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return performHugePageInvocationMap(asid, cap, cte,
                                                makeUser1stLevel(base, vmRights, attributes), lu_ret.pudSlot);
        }
    }

    case ARMPageRemap: {
        vptr_t vaddr;
        paddr_t base;
        cap_t pgdCap;
        pgde_t *pgd;
        asid_t asid;
        vm_rights_t vmRights;
        vm_page_size_t frameSize;
        vm_attributes_t attributes;
        findVSpaceForASID_ret_t find_ret;

#ifdef CONFIG_ARM_SMMU
        if (isIOSpaceFrameCap(cap)) {
            userError("ARMPageRemap: Attempting to remap frame mapped into an IOSpace");
            current_syscall_error.type = seL4_IllegalOperation;

            return EXCEPTION_SYSCALL_ERROR;
        }
#endif

        if (unlikely(length < 2 || extraCaps.excaprefs[0] == NULL)) {
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        attributes = vmAttributesFromWord(getSyscallArg(1, buffer));
        pgdCap = extraCaps.excaprefs[0]->cap;

        frameSize = cap_frame_cap_get_capFSize(cap);
        vmRights = maskVMRights(cap_frame_cap_get_capFVMRights(cap),
                                rightsFromWord(getSyscallArg(0, buffer)));

        if (unlikely(!isValidNativeRoot(pgdCap))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        pgd = PGDE_PTR(vspace_sl.top_base_ptr(pgdCap));
        asid = vspace_sl.top_cap_asid(pgdCap);

        find_ret = findVSpaceForASID(asid);
        if (unlikely(find_ret.status != EXCEPTION_NONE)) {
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(find_ret.vspace_root != pgd)) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(cap_frame_cap_get_capFMappedASID(cap) != asid)) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        base = pptr_to_paddr((void *)cap_frame_cap_get_capFBasePtr(cap));
        vaddr = cap_frame_cap_get_capFMappedAddress(cap);

        if (frameSize == ARMSmallPage) {
            lookupPTSlot_ret_t lu_ret = lookupPTSlot(pgd, vaddr);

            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return performSmallPageInvocationMap(asid, cap, cte,
                                                 makeUser3rdLevel(base, vmRights, attributes), (table_t *)lu_ret.ptSlot);

        } else if (frameSize == ARMLargePage) {
            lookupPDSlot_ret_t lu_ret = lookupPDSlot(pgd, vaddr);

            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return performLargePageInvocationMap(asid, cap, cte,
                                                 makeUser2ndLevel(base, vmRights, attributes), (dir_t *)lu_ret.pdSlot);

        } else {
            lookupPUDSlot_ret_t lu_ret = lookupPUDSlot(pgd, vaddr);

            setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
            return performHugePageInvocationMap(asid, cap, cte,
                                                makeUser1stLevel(base, vmRights, attributes), lu_ret.pudSlot);
        }
    }

    case ARMPageUnmap: {
        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);

#ifdef CONFIG_ARM_SMMU
        if (isIOSpaceFrameCap(cap)) {
            return performPageInvocationUnmapIO(cap, cte);
        } else
#endif
        {
            return performPageInvocationUnmap(cap, cte);
        }
    }

#ifdef CONFIG_ARM_SMMU
    case ARMPageMapIO: {
        return decodeARMIOMapInvocation(invLabel, length, cte, cap, extraCaps, buffer);
    }
#endif

    case ARMPageClean_Data:
    case ARMPageInvalidate_Data:
    case ARMPageCleanInvalidate_Data:
    case ARMPageUnify_Instruction: {
        vptr_t start, end;
        vptr_t vaddr;
        asid_t asid;
        word_t page_size;
        findVSpaceForASID_ret_t find_ret;

        if (length < 2) {
            userError("Page Flush: Truncated message.");
            current_syscall_error.type = seL4_TruncatedMessage;
            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(cap_frame_cap_get_capFMappedASID(cap) == 0)) {
            userError("Page Flush: Frame is not mapped.");
            current_syscall_error.type = seL4_IllegalOperation;
            return EXCEPTION_SYSCALL_ERROR;
        }

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
        /* In Hyp Mode, the mapped address is an IPA. Cache cleaning
         *  requires VA, so we need to get the IPA -> VA translation
         */
        vaddr = cap_frame_cap_get_capFBasePtr(cap);
#else
        vaddr = cap_frame_cap_get_capFMappedAddress(cap);
#endif
        asid = cap_frame_cap_get_capFMappedASID(cap);

        find_ret = findVSpaceForASID(asid);
        if (unlikely(find_ret.status != EXCEPTION_NONE)) {
            userError("Page Flush: No PGD for ASID");
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            return EXCEPTION_SYSCALL_ERROR;
        }

        start = getSyscallArg(0, buffer);
        end =   getSyscallArg(1, buffer);

        /* check that the range is sane */
        if (end <= start) {
            userError("PageFlush: Invalid range");
            current_syscall_error.type = seL4_InvalidArgument;
            current_syscall_error.invalidArgumentNumber = 1;
            return EXCEPTION_SYSCALL_ERROR;
        }

        /* start and end are currently relative inside this page */
        page_size = BIT(pageBitsForSize(cap_frame_cap_get_capFSize(cap)));
        if (start >= page_size || end > page_size) {
            userError("Page Flush: Requested range not inside page");
            current_syscall_error.type = seL4_InvalidArgument;
            current_syscall_error.invalidArgumentNumber = 0;
            return EXCEPTION_SYSCALL_ERROR;
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageFlush(invLabel, find_ret.vspace_root, asid, vaddr + start, vaddr + end - 1,
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
                                /* Stage 2 cache flushes require the Mapped Address  */
                                pptr_to_paddr((void*)cap_frame_cap_get_capFMappedAddress(cap)) + start);
#else
                                pptr_to_paddr((void*)cap_frame_cap_get_capFBasePtr(cap)) + start);
#endif
    }

    case ARMPageGetAddress:
        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performPageGetAddress(cap_frame_cap_get_capFBasePtr(cap));

    default:
        current_syscall_error.type = seL4_IllegalOperation;
        return EXCEPTION_SYSCALL_ERROR;
    }
}

exception_t
decodeARMMMUInvocation(word_t invLabel, word_t length, cptr_t cptr,
                       cte_t *cte, cap_t cap, extra_caps_t extraCaps,
                       word_t *buffer)
{
    switch (cap_get_capType(cap)) {
    case cap_page_global_directory_cap:
        return decodeARMPageGlobalDirectoryInvocation(invLabel, length, cte,
                                                      cap, extraCaps, buffer);

    case cap_page_upper_directory_cap:
        return decodeARMPageUpperDirectoryInvocation(invLabel, length, cte,
                                                     cap, extraCaps, buffer);

    case cap_page_directory_cap:
        return decodeARMPageDirectoryInvocation(invLabel, length, cte,
                                                cap, extraCaps, buffer);

    case cap_page_table_cap:
        return decodeARMPageTableInvocation (invLabel, length, cte,
                                             cap, extraCaps, buffer);

    case cap_frame_cap:
        return decodeARMFrameInvocation (invLabel, length, cte,
                                         cap, extraCaps, buffer);

    case cap_asid_control_cap: {
        unsigned int i;
        asid_t asid_base;
        word_t index, depth;
        cap_t untyped, root;
        cte_t *parentSlot, *destSlot;
        lookupSlot_ret_t lu_ret;
        void *frame;
        exception_t status;

        if (unlikely(invLabel != ARMASIDControlMakePool)) {
            current_syscall_error.type = seL4_IllegalOperation;

            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(length < 2 ||
                     extraCaps.excaprefs[0] == NULL ||
                     extraCaps.excaprefs[1] == NULL)) {
            current_syscall_error.type = seL4_TruncatedMessage;

            return EXCEPTION_SYSCALL_ERROR;
        }

        index = getSyscallArg(0, buffer);
        depth = getSyscallArg(1, buffer);
        parentSlot = extraCaps.excaprefs[0];
        untyped = parentSlot->cap;
        root = extraCaps.excaprefs[1]->cap;

        /* Find first free pool */
        for (i = 0; i < nASIDPools && armKSASIDTable[i]; i++);

        if (unlikely(i == nASIDPools)) { /* If no unallocated pool is found */
            current_syscall_error.type = seL4_DeleteFirst;

            return EXCEPTION_SYSCALL_ERROR;
        }

        asid_base = i << asidLowBits;

        if (unlikely(cap_get_capType(untyped) != cap_untyped_cap ||
                     cap_untyped_cap_get_capBlockSize(untyped) != seL4_ASIDPoolBits) ||
                cap_untyped_cap_get_capIsDevice(untyped)) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;

            return EXCEPTION_SYSCALL_ERROR;
        }

        status = ensureNoChildren(parentSlot);
        if (unlikely(status != EXCEPTION_NONE)) {
            return status;
        }

        frame = WORD_PTR(cap_untyped_cap_get_capPtr(untyped));

        lu_ret = lookupTargetSlot(root, index, depth);
        if (unlikely(lu_ret.status != EXCEPTION_NONE)) {
            return lu_ret.status;
        }
        destSlot = lu_ret.slot;

        status = ensureEmptySlot(destSlot);
        if (unlikely(status != EXCEPTION_NONE)) {
            return status;
        }

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performASIDControlInvocation(frame, destSlot, parentSlot, asid_base);
    }

    case cap_asid_pool_cap: {
        cap_t vspaceCap;
        cte_t *vspaceCapSlot;
        asid_pool_t *pool;
        unsigned int i;
        asid_t asid;

        if (unlikely(invLabel != ARMASIDPoolAssign)) {
            current_syscall_error.type = seL4_IllegalOperation;

            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(extraCaps.excaprefs[0] == NULL)) {
            current_syscall_error.type = seL4_TruncatedMessage;

            return EXCEPTION_SYSCALL_ERROR;
        }

        vspaceCapSlot = extraCaps.excaprefs[0];
        vspaceCap = vspaceCapSlot->cap;

        if (unlikely(!isVTableRoot(vspaceCap) ||
                     vspace_sl.top_cap_mapped(vspaceCap))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 1;

            return EXCEPTION_SYSCALL_ERROR;
        }

        pool = armKSASIDTable[cap_asid_pool_cap_get_capASIDBase(cap) >> asidLowBits];

        if (unlikely(!pool)) {
            current_syscall_error.type = seL4_FailedLookup;
            current_syscall_error.failedLookupWasSource = false;
            current_lookup_fault = lookup_fault_invalid_root_new();

            return EXCEPTION_SYSCALL_ERROR;
        }

        if (unlikely(pool != ASID_POOL_PTR(cap_asid_pool_cap_get_capASIDPool(cap)))) {
            current_syscall_error.type = seL4_InvalidCapability;
            current_syscall_error.invalidCapNumber = 0;

            return EXCEPTION_SYSCALL_ERROR;
        }

        /* Find first free ASID */
        asid = cap_asid_pool_cap_get_capASIDBase(cap);
        for (i = 0; i < (1 << asidLowBits) && (asid + i == 0 || pool->array[i]); i++);

        if (unlikely(i == 1 << asidLowBits)) {
            current_syscall_error.type = seL4_DeleteFirst;

            return EXCEPTION_SYSCALL_ERROR;
        }

        asid += i;

        setThreadState(NODE_STATE(ksCurThread), ThreadState_Restart);
        return performASIDPoolInvocation(asid, pool, vspaceCapSlot);
    }

    default:
        fail("Invalid ARM arch cap type");
    }
}

#ifdef CONFIG_DEBUG_BUILD
void kernelPrefetchAbort(word_t pc) VISIBLE;
void kernelDataAbort(word_t pc) VISIBLE;

void
kernelPrefetchAbort(word_t pc)
{
    word_t ifsr = getIFSR();

    printf("\n\nKERNEL PREFETCH ABORT!\n");
    printf("Faulting instruction: 0x%x\n", (unsigned int)pc);
    printf("ESR (IFSR): 0x%x\n", (unsigned int)ifsr);

    halt();
}

void
kernelDataAbort(word_t pc)
{
    word_t dfsr = getDFSR();
    word_t far = getFAR();

    printf("\n\nKERNEL DATA ABORT!\n");
    printf("Faulting instruction: 0x%lx\n", (unsigned long)pc);
    printf("FAR: 0x%lx ESR (DFSR): 0x%x\n", (unsigned long)far, (unsigned int)dfsr);

    halt();
}
#endif

#ifdef CONFIG_PRINTING
void Arch_userStackTrace(tcb_t *tptr)
{

}
#endif
