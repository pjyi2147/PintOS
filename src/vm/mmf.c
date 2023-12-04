#include "vm/mmf.h"
#include <stdio.h>
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

extern struct lock file_lock;

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
    if (page_lookup (page_tbl, upage + ofs))
    {
      return NULL;
    }
  }

  for (ofs = 0; ofs < size; ofs += PGSIZE)
  {
    uint32_t read_bytes = ofs + PGSIZE < size ? PGSIZE : size - ofs;
    page_alloc_file(upage, file, ofs, read_bytes, PGSIZE - read_bytes, true);
    upage += PGSIZE;
  }

  list_push_back(&thread_current()->mmf_list, &mmf->elem);
  return mmf;
}

struct mmf *
mmf_lookup (int mmf_id)
{
  struct list *list = &thread_current()->mmf_list;

  for (struct list_elem *e = list_begin(list); e != list_end(list); e = list_next(e))
  {
    struct mmf *f = list_entry(e, struct mmf, elem);

    if (f->id == mmf_id)
    {
      return f;
    }
  }
  return NULL;
}

void
mmf_cleanup (void)
{
    // printf("mmf_cleanup\n");
    struct thread *t = thread_current();
    struct list *list = &t->mmf_list;

    while (!list_empty(list))
    {
        struct list_elem *e = list_pop_front(list);
        struct mmf *mmf = list_entry(e, struct mmf, elem);

        file_close(mmf->file);
        free(mmf);
    }
}
