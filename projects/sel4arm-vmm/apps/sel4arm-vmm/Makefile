#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := sel4arm-vmm.bin

ENTRY_POINT := _sel4_start

ifeq ($(ARM_HYP),)
ifeq ($(AARCH64_HYP),)
$(error sel4arm-vmm can only be build with hypervisor kernel)
endif
endif

ifeq ($(CONFIG_PLAT_ZYNQMP),y)
else ifeq ($(CONFIG_PLAT_IMX8),y)
else ifeq ($(CONFIG_PLAT_QEMU),y)
else
$(error Unknown platform)
endif

ifeq (${COMPONENTS},)
$(error "No compoments setup to build against.")
endif

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
CFILES   += $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/$(PLAT)/*.c))
ASMFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.S))
OFILES   := archive.o

HDRFILES := $(wildcard $(SOURCE_DIR)/src/*.h) \
    $(wildcard ${SOURCE_DIR}/src/${PLAT}/*.h)

# Libraries required to build the target
LIBS = c sel4 sel4muslcsys sel4vka sel4allocman \
       platsupport sel4platsupport sel4vspace elf \
       sel4debug sel4utils sel4simple utils  sel4simple-default cpio \
       sel4arm-vmm sel4dma

include $(SEL4_COMMON)/common.mk


${COMPONENTS}:
	false

archive.o: ${COMPONENTS}
	$(Q)mkdir -p $(dir $@)
	${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive $^
