#
# Copyright 2014, General Dynamics C4 Systems
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(GD_GPL)
#

DIRECTORIES += src/arch/arm/machine

ARCH_C_SOURCES += machine/cache.c \
                  machine/errata.c \
                  machine/io.c \
                  machine/smmu.c \
                  machine/debug.c

ifeq ($(CPU), cortex-a9)
    ARCH_C_SOURCES += machine/l2c_310.c

ifeq ($(PLAT), exynos4)
    # if exynos4412 has MPCORE peripherals, then
    # samsung has NOT documented their location
else
    ARCH_C_SOURCES += machine/priv_timer.c
endif

endif

# platforms that use the generic timer
ifeq ($(PLAT), $(filter $(PLAT), exynos5 bcm2837 hikey tk1 tx1 zynqmp imx7 imx8))
	ARCH_C_SOURCES += machine/generic_timer.c
endif

ifeq ($(GIC), 390)
    ARCH_C_SOURCES += machine/gic_pl390.c
endif

ifeq ($(GIC), 500)
    ARCH_C_SOURCES += machine/gic_500.c
endif
