#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"

struct frame
{
    void *upage;               /* User virtual address. */
    void *kpage;               /* Kernel virtual address. */

    struct thread *thread;     /* Thread that owns this frame. */
    struct list_elem elem;     /* List element. */
};

void frame_init (void);

void *frame_alloc (enum palloc_flags flags, void *upage);
void frame_free (void *kpage);

void frame_table_cleanup(void);

#endif /* VM_FRAME_H */
