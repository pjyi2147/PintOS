#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "filesys/off_t.h"

enum page_status
{
  PAGE_STATUS_ZERO,
  PAGE_STATUS_FRAME,
  PAGE_STATUS_FILE,
  PAGE_STATUS_SWAP
};

struct page
{
  void *upage;
  void *kpage;

  struct hash_elem elem;

  enum page_status status;
  enum page_status origin;

  struct file *file;
  off_t ofs;
  uint32_t read_bytes, zero_bytes;
  bool writable;
  int swap_index;
};

void page_table_init (struct hash *page_table);
void page_table_destroy (struct hash *page_table);
void page_init (struct hash *page_table, void *upage, void *kpage);
void page_zero_init (struct hash *page_table, void *upage);
void page_frame_init (struct hash *page_table, void *upage, void *kpage);
struct page* page_file_init (struct hash *page_table, void *upage,
                              struct file *file, off_t ofs,
                              uint32_t read_bytes, uint32_t zero_bytes,
                              bool writable);
bool page_load (struct hash *page_table, void *upage);
struct page* page_get (struct hash *page_table, void *upage);
void page_delete (struct hash *page_table, struct page *p);

#endif /* VM_PAGE_H */
