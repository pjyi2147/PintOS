#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "vm/page.h"

void swap_table_init(void);
void swap_in(struct page *p, void *kva);
int swap_out(void *kva);

#endif /* VM_SWAP_H */
