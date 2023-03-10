/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(GD_GPL)
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

#include <config.h>
#include <assert.h>
#include <stdint.h>

#if defined(CONFIG_ARCH_AARCH32)
compile_assert(long_is_32bits, sizeof(unsigned long) == 4)
#elif defined(CONFIG_ARCH_AARCH64)
compile_assert(long_is_64bits, sizeof(unsigned long) == 8)
#endif

typedef unsigned long word_t;
typedef signed long sword_t;
typedef word_t vptr_t;
typedef word_t paddr_t;
typedef word_t pptr_t;
typedef word_t cptr_t;
typedef word_t node_id_t;
typedef word_t cpu_id_t;
typedef word_t dom_t;

typedef uint8_t  hw_asid_t;

enum hwASIDConstants {
    hwASIDMax = 255,
    hwASIDBits = 8
};

/* for libsel4 headers that the kernel shares */
typedef word_t seL4_Word;
typedef cptr_t seL4_CPtr;
typedef uint32_t seL4_Uint32;
typedef uint16_t seL4_Uint16;
typedef uint8_t seL4_Uint8;
typedef node_id_t seL4_NodeId;
typedef dom_t seL4_Domain;
typedef paddr_t seL4_PAddr;

typedef struct kernel_frame {
    paddr_t paddr;
    pptr_t pptr;
    word_t nr_pages;
    int armExecuteNever;
} kernel_frame_t;

#endif
