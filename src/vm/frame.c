#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static struct list frame_table;
static struct lock frame_lock;

void
frame_init(void)
{
  lock_init(&frame_lock);
  list_init(&frame_table);
}

void *
frame_alloc(enum palloc_flags flags, void *upage)
{
  void *frame = palloc_get_page(flags);
  if (frame == NULL)
  {
    frame = frame_evict(flags, upage);
  }
  struct frame *f = malloc(sizeof(struct frame));
  f->kpage = frame;
  f->upage = upage;
  f->thread = thread_current();
  list_push_back(&frame_table, &f->elem);
  return frame;
}

void
frame_free(void *frame)
{
  struct list_elem *e;
  struct frame *fe;
  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
  {
    fe = list_entry(e, struct frame, elem);
    if (fe->kpage == frame)
    {
      list_remove(e);
      free(fe);
      palloc_free_page(frame);
      return;
    }
  }
}

void
frame_free_all(struct thread *t)
{
  struct list_elem *e;
  struct frame *fe;
  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
  {
    fe = list_entry(e, struct frame, elem);
    if (fe->thread == t)
    {
      list_remove(e);
      free(fe);
      palloc_free_page(fe->kpage);
    }
  }
}
