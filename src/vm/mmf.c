#include "vm/mmf.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

struct mmf *
mmf_init (int id, struct file* file, void* upage)
{
  struct mmf *mmf = (struct mmf *)malloc(sizeof mmf);

  mmf->id = id;
  mmf->file = file;
  mmf->upage = upage;

  off_t ofs;
  int size = file_length(file);

  struct hash *page_tbl = &thread_current()->page_table;

  for (ofs = 0; ofs < size; ofs += PGSIZE)
  {
    if (page_get (page_tbl, upage + ofs) != NULL)
    {
      return NULL;
    }
  }

  for (ofs = 0; ofs < size; ofs += PGSIZE)
  {
    uint32_t read_bytes = ofs + PGSIZE < size ? PGSIZE : size - ofs;
    page_file_init(page_tbl, upage, file, ofs, read_bytes, PGSIZE - read_bytes, true);
    upage += PGSIZE;
  }
  // printf("mmf_init: mmf->upage %p, mmf->file %p\n", (void *)mmf->upage, mmf->file);
  list_push_back(&thread_current()->mmf_list, &mmf->elem);
  return mmf;
}

struct mmf *
mmf_get (int mmf_id)
{
  struct list *list = &thread_current()->mmf_list;

  for (struct list_elem *e = list_begin(list); e != list_end(list); e = list_next(e))
  {
    struct mmf *f = list_entry(e, struct mmf, elem);

    if (f->id == mmf_id)
    {
      // printf("mmf_get: found mmf, upage %p, file %p\n", (f->upage), f->file);
      // f->upage = *(void **)(f->upage);
      return f;
    }
  }
  return NULL;
}

void
mmf_cleanup()
{
  struct thread *cur = thread_current();
  while (!list_empty(&cur->mmf_list))
  {
    struct mmf *f = list_entry(list_pop_front(&cur->mmf_list), struct mmf, elem);
    file_close(f->file);
    free(f);
  }
}
