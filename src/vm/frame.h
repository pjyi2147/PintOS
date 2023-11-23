#ifndef _VM_FRAME_H_
#define _VM_FRAME_H_

#include <debug.h>
#include <list.h>
#include <stdint.h>

#include "threads/synch.h"

struct frame
{
  void *kpage;                  /* Kernel virtual address. */
  void *upage;                  /* User virtual address. */
  struct thread *thread;        /* Thread that owns the frame. */
  struct list_elem elem;        /* List element. */
};

void frame_init (void);
void *frame_alloc (enum palloc_flags flags, void *upage);
void frame_free (void *kpage);
void frame_free_all (struct thread *t);

#endif /* _VM_FRAME_H_ */
