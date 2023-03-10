--
-- Copyright 2017, Data61
-- Commonwealth Scientific and Industrial Research Organisation (CSIRO)
-- ABN 41 687 119 230.
--
-- Copyright 2018, DornerWorks
--
-- This software may be distributed and modified according to the terms of
-- the GNU General Public License version 2. Note that NO WARRANTY is provided.
-- See "LICENSE_GPLv2.txt" for details.
--
-- @TAG(DATA61_DORNERWORKS_GPL)
--

#include <config.h>

-- Default base size: uint64_t
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
base 64(48,0)
#else
base 64(48,1)
#endif

-- Including the common structures_64.bf is neccessary because
-- we need the structures to be visible here when building
-- the capType
#include <object/structures_64.bf>
#include <config.h>

---- ARM-specific caps

block frame_cap {
#ifdef CONFIG_ARM_SMMU
    padding                          1
    field capFMappedASID             15
#else
    field capFMappedASID             16
#endif
    field_high capFBasePtr           48

    field capType                    5
    field capFSize                   2
    field_high capFMappedAddress     48
    field capFVMRights               2
#ifdef CONFIG_ARM_SMMU
    field capFIsIOSpace              1
#else
    padding                          1
#endif
    field capFIsDevice               1
    padding                          5
}

-- Forth-level page table
block page_table_cap {
    field capPTMappedASID            16
    field_high capPTBasePtr          48

    field capType                    5
    padding                          10
    field capPTIsMapped              1
    field_high capPTMappedAddress    28
    padding                          20
}

-- Third-level page table (page directory)
block page_directory_cap {
    field capPDMappedASID            16
    field_high capPDBasePtr          48

    field capType                    5
    padding                          10
    field capPDIsMapped              1
    field_high capPDMappedAddress    19
    padding                          29
}

-- Second-level page table (page upper directory)
block page_upper_directory_cap {
    field capPUDMappedASID           16
    field_high capPUDBasePtr         48

    field capType                    5
    field capPUDIsMapped             1
    field_high capPUDMappedAddress   10
    padding                          48
}

-- First-level page table (page global directory)
block page_global_directory_cap {
    field capPGDMappedASID           16
    field_high capPGDBasePtr         48

    field capType                    5
    field capPGDIsMapped             1
    padding                          58
}

-- Cap to the table of 2^7 ASID pools
block asid_control_cap {
    padding                          64

    field capType                    5
    padding                          59
}

-- Cap to a pool of 2^9 ASIDs
block asid_pool_cap {
    padding                         64

    field capType                   5
    field capASIDBase               16
    padding                         6
    field_high capASIDPool          37
}

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
block vcpu_cap {
    field capVCPUPtr                64

    field capType                   5
    padding                         59
}
#endif

#ifdef CONFIG_ARM_SMMU_V2
-- IO space caps
block io_space_cap {
    field   capStreamID    16
    padding                48

    field   capType         5
    padding                59
}

block io_space_capdata {
    padding                48

    field streamID         16
}
#endif

#ifdef CONFIG_ARM_SMMU
block io_page_table_cap (capType, capIOPTIsMapped, capIOPTASID, capIOPTLevel, capIOPTBasePtr, capIOPTMappedAddress) {
    field_high  capIOPTBasePtr          48
    padding                             16

    field capType                       5
    padding                             11
    field_high  capIOPTMappedAddress    28
    field       capIOPTLevel            4
    field       capIOPTASID             7
    field       capIOPTIsMapped         1
    padding                             8
}
#endif

-- NB: odd numbers are arch caps (see isArchCap())
tagged_union cap capType {

    -- 5-bit tag caps
    tag null_cap                    0
    tag untyped_cap                 2
    tag endpoint_cap                4
    tag notification_cap            6
    tag reply_cap                   8
    tag cnode_cap                   10
    tag thread_cap                  12
    tag irq_control_cap             14
    tag irq_handler_cap             16
    tag zombie_cap                  18
    tag domain_cap                  20

    -- 5-bit tag arch caps
    tag frame_cap                   1
    tag page_table_cap              3
    tag page_directory_cap          5
    tag page_upper_directory_cap    7
    tag page_global_directory_cap   9
    tag asid_control_cap            11
    tag asid_pool_cap               13

    -- we use the same names as for x86 IOMMU caps
#ifdef CONFIG_ARM_SMMU
    tag io_space_cap                15
    tag io_page_table_cap           17
#endif

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    tag vcpu_cap                    21
#endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */
}

---- Arch-independent object types

block VMFault {
    field address                   64
    field FSR                       32
    field instructionFault          1
    padding                         27
    field seL4_FaultType            4
}

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT

block VGICMaintenance {
    padding         64
    field idx        6
    field idxValid   1
    padding         25
    padding         28
    field seL4_FaultType  4
}

block VCPUFault {
    padding         64
    field hsr       32
    padding         28
    field seL4_FaultType  4
}

block PSCIFault {
    padding         64
    field function  32
    padding         28
    field seL4_FaultType  4
}

#endif

-- VM attributes

block vm_attributes {
    padding                         61
    field armExecuteNever           1
    field armParityEnabled          1
    field armPageCacheable          1
}


#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT

#ifdef CONFIG_HAVE_GIC_500

block virq_invalid {
    field virqType      2
    padding             20
    field virqEOIIRQEN  1
    padding             41
}

block virq_active {
    field virqType      2
    padding             20
    field virqEOIIRQEN  1
    padding             41
}

block virq_pending {
    field virqType      2
    padding             1
    field virqGroup     1
    padding             4
    field virqPriority  8
    padding             6
    field virqEOIIRQEN  1
    padding             9
    field virqIRQ       32
}

#else

block virq_invalid {
    padding             34
    field virqType      2
    padding             8
    field virqEOIIRQEN  1
    padding             19
}

block virq_active {
    padding             34
    field virqType      2
    padding             8
    field virqEOIIRQEN  1
    padding             19
}

block virq_pending {
    padding             33
    field virqGroup     1
    field virqType      2
    field virqPriority  5
    padding             3
    field virqEOIIRQEN  1
    padding             9
    field virqIRQ       10
}

#endif

tagged_union virq virqType {
    tag virq_invalid    0
    tag virq_pending    1
    tag virq_active     2
}

#endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */

---- ARM-specific object types

-- PGDE, PUDE, PDEs and PTEs, assuming 48-bit physical address
base 64(48,0)

block pgde {
    padding                         16
    field_high pud_base_address     36
    padding                         10
    field reserved                  2 -- must be 0b11
}

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT

block pteS2 {
    padding                         9
    field XN                        2
    field Contig                    1
    field DBM                       1
    padding                         3
    field_high page_base_address    36
    padding                         1
    field AF                        1
    field SH                        2
    field S2AP                      2
    field MemAttr                   4
    field reserved                  2 -- 0b11 for 4kB page, 0b01 for 2MB/1GB Blocks
}

#endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */

block pude_1g {
    padding                         9
    field UXN                       1
    padding                         6
    field_high page_base_address    18
    padding                         18
    field nG                        1
    field AF                        1
    field SH                        2
    field AP                        2
    padding                         1
    field AttrIndx                  3
    field pude_type                 2
}

block pude_pd {
    padding                         16
    field_high pd_base_address      36
    padding                         10
    field pude_type                 2
}

tagged_union pude pude_type {
    tag pude_1g                     1
    tag pude_pd                     3
}

block pde_large {
    padding                         9
    field UXN                       1
    padding                         6
    field_high page_base_address    27
    padding                         9
    field nG                        1
    field AF                        1
    field SH                        2
    field AP                        2
    padding                         1
    field AttrIndx                  3
    field pde_type                  2
}

block pde_small {
    padding                         16
    field_high pt_base_address      36
    padding                         10
    field pde_type                  2
}

tagged_union pde pde_type {
    tag pde_large                   1
    tag pde_small                   3
}

block pte {
    padding                         9
    field UXN                       1
    padding                         6
    field_high page_base_address    36
    field nG                        1
    field AF                        1
    field SH                        2
    field AP                        2
    padding                         1
    field AttrIndx                  3
    field reserved                  2 -- must be 0b11
}

block ttbr {
    field asid                      16
    field_high base_address         48
}

#ifdef CONFIG_ARM_SMMU_V2
block iopde {
    padding                         16
    field_high address              36
    padding                         10
    field pde_type                  2
}

#ifdef CONFIG_SMMU_S1_TRANS
block iopte {
    padding                      9
    field XN                     1
    field PXN                    1
    padding                      5
    field_high address           36
    field nG                     1
    field AF                     1
    field SH                     2
    field AP                     2
    field NS                     1
    field AttrIndx               3
    field pteType                2
}
#else
block iopte {
    padding                      9
    field XN                     1
    padding                      6
    field_high address           36
    padding                      1
    field AF                     1
    field SH                     2
    field S2AP                   2
    field Attr                   4
    field pteType                2
}
#endif
#endif

#include <arch/api/shared_types.bf>
