#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DATA61_GPL)
#
# Targets
TARGETS := $(notdir ${SOURCE_DIR}).a

# Source files required to build the target
CFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/*.c))
CFILES += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/arch-$(ARCH)/*.c))

# Header files/directories this library provides
HDRFILES := \
	$(wildcard ${SOURCE_DIR}/include/*) \
	$(wildcard ${SOURCE_DIR}/arch_include/$(ARCH)/*)

include $(SEL4_COMMON)/common.mk
