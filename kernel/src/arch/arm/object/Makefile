#
# Copyright 2014, General Dynamics C4 Systems
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(GD_GPL)
#

DIRECTORIES += src/arch/arm/object

ARCH_C_SOURCES += object/interrupt.c object/tcb.c object/vcpu.c

ifneq ($(PLAT), tk1)
    ARCH_C_SOURCES += object/iospace.c
endif
