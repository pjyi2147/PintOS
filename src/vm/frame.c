#include "vm/frame.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct list frame_table;
static struct lock frame_lock;
static struct frame *last_frame;
extern struct lock file_lock;

void frame_evict(void);

// Frame table Initialization
void
frame_init(void)
{
  lock_init(&frame_lock);
  list_init(&frame_table);
  last_frame = NULL;
}

// Allocate frame
void *
frame_alloc(enum palloc_flags flags, void *upage)
{
  lock_acquire(&frame_lock);
  void *kpage = palloc_get_page(flags);
  if (kpage == NULL)
  {
    // printf("frame_alloc: eviction\n");
    frame_evict();
    kpage = palloc_get_page(flags);
    if (kpage == NULL)
    {
      // printf("frame_alloc: eviction failed\n");
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

// Free frame with kpage
void
frame_free(void *kpage)
{
  struct list_elem *e;
  struct frame *fe;
  lock_acquire(&frame_lock);
  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
  {
    fe = list_entry(e, struct frame, elem);
    if (fe->kpage == kpage)
    {
      list_remove(e);
      palloc_free_page(kpage);
      pagedir_clear_page(fe->thread->pagedir, fe->upage);
      free(fe);
      lock_release(&frame_lock);
      return;
    }
  }
  lock_release(&frame_lock);
  // if there is no frame to free then it is an error
  syscall_exit(-1);
}

// Get frame
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

// Evict frame
void
frame_evict(void)
{
  ASSERT(lock_held_by_current_thread(&frame_lock));

  struct frame *fe = last_frame;
  struct page *pe;

  if (fe == NULL)
  {
    fe = list_entry(list_front(&frame_table), struct frame, elem);
  }

  while (pagedir_is_accessed(fe->thread->pagedir, fe->upage))
  {
    pagedir_set_accessed(fe->thread->pagedir, fe->upage, false);
    if (list_next(&fe->elem) == list_end(&frame_table))
    {
      fe = list_entry(list_front(&frame_table), struct frame, elem);
    }
    else
    {
      fe = list_entry(list_next(&fe->elem), struct frame, elem);
    }
  }

  if (list_next(&fe->elem) == list_end(&frame_table))
  {
    last_frame = list_entry(list_front(&frame_table), struct frame, elem);
  }
  else
  {
    last_frame = list_entry(list_next(&fe->elem), struct frame, elem);
  }

  // printf("frame_evict: page_get %p\n", fe->upage);
  pe = page_get (&fe->thread->page_table, fe->upage);
  if (pe == NULL)
  {
    // printf("frame_evict: page_get failed\n");
    lock_release(&frame_lock);
    syscall_exit(-1);
  }

  if (pe->origin == PAGE_STATUS_FILE &&
      pagedir_is_dirty(fe->thread->pagedir, fe->upage))
  {
    // printf("frame_evict: file write; dirty\n");
    if (!lock_held_by_current_thread(&file_lock))
    {
      lock_acquire(&file_lock);
    }
    file_write_at(pe->file, fe->kpage, pe->read_bytes, pe->ofs);
    lock_release(&file_lock);
  }

  // printf("frame_evict: swap_out\n");
  pe->swap_index = swap_out(fe->kpage);
  pe->status = PAGE_STATUS_SWAP;

  pagedir_clear_page(fe->thread->pagedir, fe->upage);

  lock_release(&frame_lock);
  frame_free(fe->kpage);
  lock_acquire(&frame_lock);
}
