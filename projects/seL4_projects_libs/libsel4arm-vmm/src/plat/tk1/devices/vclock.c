/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <stdlib.h>
#include <string.h>

#include <sel4arm-vmm/devices.h>

const struct device dev_clkcar = {
    .devid = DEV_CUSTOM,
    .name = "clkcar",
    .pstart = TK1_CLKCAR_PADDR,
    .size = PAGE_SIZE,
    .handle_page_fault = NULL,
    .priv = NULL
};
