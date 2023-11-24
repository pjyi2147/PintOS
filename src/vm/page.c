#include "vm/page.h"
#include <hash.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"

static hash_hash_func page_hash_func;
static hash_less_func page_less_func;
static void page_destructor (struct hash_elem *e, void *aux);
extern struct lock file_lock;

// Page table initialization
void
page_table_init (struct hash *page_table)
{
  hash_init (page_table, page_hash_func, page_less_func, NULL);
}

//' Page table destruction
void
page_table_destroy (struct hash *page_table)
{
  hash_destroy (page_table, page_destructor);
}

// Zero page initialization
void
page_zero_init (struct hash *page_table, void *upage)
{
  struct page *p = malloc (sizeof (struct page));
  p->kpage = NULL;
  p->upage = upage;

  p->status = PAGE_STATUS_ZERO;
  p->origin = PAGE_STATUS_ZERO;

  p->file = NULL;
  p->writable = true;

  hash_insert (page_table, &p->elem);
}

// Frame page initialization
void
page_frame_init (struct hash *page_table, void *upage, void *kpage)
{
  struct page *p = malloc (sizeof (struct page));
  p->kpage = kpage;
  p->upage = upage;

  p->status = PAGE_STATUS_FRAME;
  p->origin = PAGE_STATUS_FRAME;

  p->file = NULL;
  p->writable = true;

  hash_insert (page_table, &p->elem);
}

// File page initialization
struct page*
page_file_init (struct hash *page_table, void *upage,
                struct file *file, off_t ofs, uint32_t read_bytes,
                uint32_t zero_bytes, bool writable)
{
  struct page *p = malloc (sizeof (struct page));
  p->kpage = NULL;
  p->upage = upage;

  p->status = PAGE_STATUS_FILE;
  p->origin = PAGE_STATUS_FILE;

  p->file = file;
  p->ofs = ofs;
  p->read_bytes = read_bytes;
  p->zero_bytes = zero_bytes;
  p->writable = writable;

  hash_insert (page_table, &p->elem);

  return p;
}

bool
page_load (struct hash *page_table, void *upage)
{
  struct page *p = page_get (page_table, upage);
  if (p == NULL)
  {
    return false;
  }

  void *kpage = frame_alloc (PAL_USER, upage);
  if (kpage == NULL)
  {
    return false;
  }

  switch (p->status)
  {
    case PAGE_STATUS_ZERO:
      memset (kpage, 0, PGSIZE);
      break;
    case PAGE_STATUS_SWAP:
      // printf("page_load: swap in %p\n", upage);
      swap_in (p, kpage);
      // hex_dump (upage, kpage, 32, true);
      break;
    case PAGE_STATUS_FILE:
      //printf("page_load: file read %p\n", upage);
      uint32_t file_read_bytes = file_read_at(p->file, kpage, p->read_bytes, p->ofs);
      if (file_read_bytes != p->read_bytes)
      {
        frame_free (p->kpage);
        return false;
      }
      memset (kpage + file_read_bytes, 0, p->zero_bytes);
      break;
    // case PAGE_STATUS_FRAME: it is already loaded to frame! error!
    default:
      return false;
  }

  if (pagedir_get_page (thread_current ()->pagedir, upage) == NULL
      && !pagedir_set_page (thread_current ()->pagedir, upage, kpage, p->writable))
  {
    // printf("page_load: pagedir_set_page failed\n");
    frame_free (p->kpage);
    return false;
  }

  p->kpage = kpage;
  p->status = PAGE_STATUS_FRAME;
  return true;
}

struct page* page_get (struct hash *page_table, void *upage)
{
  struct page p;
  struct hash_elem *e;

  p.upage = upage;
  e = hash_find (page_table, &p.elem);

  return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}

void
page_delete (struct hash *page_table, struct page *p)
{
  hash_delete (page_table, &p->elem);
  free (p);
}

static unsigned
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  struct page *p = hash_entry (e, struct page, elem);
  return hash_bytes (&p->upage, sizeof (p->upage));
}

static bool
page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED)
{
  struct page *pa = hash_entry (a, struct page, elem);
  struct page *pb = hash_entry (b, struct page, elem);
  return pa->upage < pb->upage;
}

static void
page_destructor (struct hash_elem *e, void *aux UNUSED)
{
  struct page *p = hash_entry (e, struct page, elem);

  if (p->status == PAGE_STATUS_FRAME)
  {
    if (p->origin == PAGE_STATUS_FILE && pagedir_is_dirty (thread_current ()->pagedir, p->upage))
    {
      if (!lock_held_by_current_thread(&file_lock))
      {
        lock_acquire(&file_lock);
      }
      file_write_at (p->file, p->kpage, p->read_bytes, p->ofs);
      lock_release(&file_lock);
    }
    frame_free (p->kpage);
  }

  free (p);
}
