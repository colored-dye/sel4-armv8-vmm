/*
 * Copyright 2017, DornerWorks
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_DORNERWORKS_GPL)
 */
/*
 * This data was produced by DornerWorks, Ltd. of Grand Rapids, MI, USA under
 * a DARPA SBIR, Contract Number D16PC00107.
 *
 * Approved for Public Release, Distribution Unlimited.
 */

#ifndef __PLAT_MODE_MACHINE_HARDWARE_H
#define __PLAT_MODE_MACHINE_HARDWARE_H

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
#define kernelBase          0xff8000000000
#else
#define kernelBase          0xffffff8000000000
#endif

/* Maximum virtual address accessible from userspace */
#define USER_TOP            0x00007fffffffffff

#endif /* __PLAT_MODE_MACHINE_HARDWARE_H */
