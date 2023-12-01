#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"

enum PAGE_STATUS
{
    PAGE_STATUS_FRAME,
    PAGE_STATUS_ZERO,
    PAGE_STATUS_SWAP,
    PAGE_STATUS_FILE
};

struct page
{
    void *upage;               /* User virtual address. */
    void *kpage;               /* Kernel virtual address. */

    struct thread *thread;     /* Thread that owns this frame. */

    struct hash_elem elem;     /* List element. */

    enum PAGE_STATUS status;   /* Status of the page. */
    enum PAGE_STATUS origin;   /* Original status of the page. */

    int swap_index;            /* Swap index of the page. */

    struct file *file;         /* File that owns this page. */
    off_t ofs;                 /* Offset of the page in the file. */
    uint32_t read_bytes;       /* Number of bytes to read from the file. */
    uint32_t zero_bytes;       /* Number of zero bytes to add to the file. */
    bool writable;             /* True if the page is writable. */
};

void page_table_init (struct hash *page_table);
void page_table_destroy (struct hash *page_table);

void page_alloc_frame (void *upage, void *kpage);
void page_alloc_zero (void *upage);
void page_alloc_file (void *upage, struct file *file, off_t ofs,
                      uint32_t read_bytes, uint32_t zero_bytes, bool writable);

void page_free (void *upage);

struct page *page_lookup (void *upage);

bool page_load (void *fault_page);

#endif /* VM_PAGE_H */
