/*
 * Copyright 2016, General Dynamics C4 Systems
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(GD_GPL)
 */

#ifndef __ARCH_OBJECT_IOSPACE_H
#define __ARCH_OBJECT_IOSPACE_H

#include <types.h>
#include <api/failures.h>
#include <object/structures.h>

#ifdef CONFIG_ARM_SMMU

#ifdef CONFIG_ARM_SMMU_V2
cap_t master_iospace_cap(void);

static inline seL4_SlotRegion
create_iospace_caps(cap_t root_cnode_cap)
{
    return S_REG_EMPTY;
}
#else
static inline cap_t
master_iospace_cap(void)
{
    return cap_null_cap_new();
}

seL4_SlotRegion create_iospace_caps(cap_t root_cnode_cap);
#endif

exception_t decodeARMIOPTInvocation(word_t invLabel, uint32_t length, cte_t* slot, cap_t cap, extra_caps_t excaps, word_t* buffer);
exception_t decodeARMIOMapInvocation(word_t invLabel, uint32_t length, cte_t* slot, cap_t cap, extra_caps_t excaps, word_t* buffer);
exception_t performPageInvocationUnmapIO(cap_t cap, cte_t *slot);
exception_t decodeARMIOSpaceInvocation(word_t invLabel, cap_t cap);
void unmapIOPage(cap_t cap);
void deleteIOPageTable(cap_t cap);
void clearIOPageDirectory(cap_t cap);

#else

/* define dummy functions */
static inline int
plat_smmu_init(void)
{
    return 0;
}

static inline cap_t
master_iospace_cap(void)
{
    return cap_null_cap_new();
}

static inline seL4_SlotRegion
create_iospace_caps(cap_t root_cnode_cap)
{
    return S_REG_EMPTY;
}

static inline exception_t
decodeARMIOPTInvocation(word_t invLabel, uint32_t length, cte_t* slot, cap_t cap, extra_caps_t excaps, word_t* buffer)
{
    return EXCEPTION_NONE;
}

static inline exception_t
decodeARMIOMapInvocation(word_t invLabel, uint32_t length, cte_t* slot, cap_t cap, extra_caps_t excaps, word_t* buffer)
{
    return EXCEPTION_NONE;
}

static inline exception_t
performPageInvocationUnmapIO(cap_t cap, cte_t *slot)
{
    return EXCEPTION_NONE;
}

static inline exception_t
decodeARMIOSpaceInvocation(word_t invLabel, cap_t cap)
{
    return EXCEPTION_NONE;
}

static inline void
unmapIOPage(cap_t cap)
{
}

static inline void
deleteIOPageTable(cap_t cap)
{
}

static inline void
clearIOPageDirectory(cap_t cap)
{
}

#endif /* end of !CONFIG_ARM_SMMU */

#endif

