#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame
{
  void *kpage;                  /* Kernel virtual address. */
  void *upage;                  /* User virtual address. */
  struct thread *thread;        /* Thread that owns the frame. */
  struct list_elem elem;        /* List element. */
};

void frame_init (void);
void *frame_alloc (enum palloc_flags flags, void *upage);
void frame_free(void *kpage);
struct frame* frame_get (void *kpage);

#endif /* VM_FRAME_H */
