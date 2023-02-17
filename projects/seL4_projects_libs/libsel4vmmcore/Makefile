TARGETS := libsel4vmmcore.a

CFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/util/*.c))

HDRFILES := $(wildcard ${SOURCE_DIR}/include/sel4vmmcore/util/*)

include $(SEL4_COMMON)/common.mk
