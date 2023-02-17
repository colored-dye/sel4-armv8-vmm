/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include "vm.h"

#ifdef CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
int reg_new_vchan_con(vm_t *vmm, camkes_vchan_con_t *con)
{
    int dom_num = con->dest_dom_number;
    unsigned int *hnum = &vmm->vchan_num_cons;
    if (get_vchan_con(vmm, dom_num) != NULL) {
        return -1;
    }

    vmm->vchan_cons = realloc(vmm->vchan_cons, sizeof(camkes_vchan_con_t) * (*hnum + 1));
    if (vmm->vchan_cons == NULL) {
        return -1;
    }

    vmm->vchan_cons[*hnum] = con;
    vmm->vchan_num_cons++;

    return 0;
}

camkes_vchan_con_t *get_vchan_con(vm_t *vmm, int con_dom_num)
{
    for (int i = 0; i < vmm->vchan_num_cons; i++) {
        camkes_vchan_con_t *con = vmm->vchan_cons[i];
        if (con->dest_dom_number == con_dom_num) {
            return con;
        }
    }
    return NULL;
}
#endif //CONFIG_LIB_SEL4_ARM_VMM_VCHAN_SUPPORT
