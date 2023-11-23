#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct list frame_table;
static struct lock frame_lock;
static struct frame *last_frame;

void
frame_init(void)
{
  lock_init(&frame_lock);
  list_init(&frame_table);
}

void *
frame_alloc(enum palloc_flags flags, void *upage)
{
  lock_acquire(&frame_lock);
  void *kpage = palloc_get_page(flags);
  if (kpage == NULL)
  {
    frame_evict(flags, upage);
    kpage = palloc_get_page(flags);
    if (kpage == NULL)
    {
      return NULL;
    }
  }
  struct frame *f = malloc(sizeof(struct frame));
  f->kpage = kpage;
  f->upage = upage;
  f->thread = thread_current ();
  list_push_back(&frame_table, &f->elem);
  lock_release(&frame_lock);
  return kpage;
}

void
frame_free(void *frame)
{
  struct list_elem *e;
  struct frame *fe;
  lock_acquire(&frame_lock);
  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
  {
    fe = list_entry(e, struct frame, elem);
    if (fe->kpage == frame)
    {
      list_remove(e);
      palloc_free_page(frame);
      pagedir_clear_page(fe->thread->pagedir, fe->upage);
      free(fe);
      lock_release(&frame_lock);
      return;
    }
  }
  syscall_exit(-1);
}

struct frame *
frame_get (void *kpage)
{
  struct list_elem *e;
  struct frame *fe;
  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
  {
    fe = list_entry(e, struct frame, elem);
    if (fe->kpage == kpage)
    {
      return fe;
    }
  }
  return NULL;
}

void
frame_evict(enum palloc_flags flags, void *upage)
{
  ASSERT(lock_held_by_current_thread(&frame_lock));

  struct frame *fe = last_frame;
  struct page *pe;

  do
  {
    if (fe != NULL)
    {
      pagedir_set_accessed(fe->thread->pagedir, fe->upage, false);
    }

    if (last_frame == NULL || list_next(&fe->elem) == list_end(&frame_table))
    {
      fe = list_entry(list_begin(&frame_table), struct frame, elem);
    }
    else
    {
      fe = list_entry(list_next(&fe->elem), struct frame, elem);
    }
  } while (!pagedir_is_accessed(fe->thread->pagedir, fe->upage));

  // TODO: swap out the frame
  pe = page_get (fe->thread->page_table, fe->upage);
  pe->status = PAGE_STATUS_SWAP;
  pe->swap_index = swap_out(fe->kpage);

  frame_free(fe->kpage);
  lock_release(&frame_lock);
}
