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

#ifndef __PLAT_MODE_MACHINE_DEVICES_H
#define __PLAT_MODE_MACHINE_DEVICES_H

/* These devices are used by the seL4 kernel. */
#ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
#define UART_PPTR                   0xffffffffffff0000
#define GIC_DISTRIBUTOR_PPTR        0xffffffffffff3000
#define GIC_CONTROLLER_PPTR         0xffffffffffff4000
#define GIC_VCPUCTRL_PPTR           0xffffffffffff6000
#define SMMU_PPTR                   0xfffffffffff20000
#else
#define UART_PPTR                   0xffffffff0000
#define GIC_DISTRIBUTOR_PPTR        0xffffffff3000
#define GIC_CONTROLLER_PPTR         0xffffffff4000
#define GIC_VCPUCTRL_PPTR           0xffffffff6000
#define SMMU_PPTR                   0xfffffff20000
#endif

#endif /* __PLAT_MODE_MACHINE_DEVICES_H */
