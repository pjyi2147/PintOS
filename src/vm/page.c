#include "vm/page.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"

static hash_hash_func page_hash_func;
static hash_less_func page_less_func;
static void page_destructor (struct hash_elem *e, void *aux);
extern struct lock file_lock;

void
page_table_init (struct hash *page_table)
{
    hash_init (page_table, page_hash_func, page_less_func, NULL);
}

void
page_table_destroy (struct hash *page_table)
{
    hash_destroy (page_table, page_destructor);
}

void
page_alloc_frame (void *upage, void *kpage)
{
    // printf("page_alloc_frame\n");
    struct page *p = malloc( sizeof (struct page));
    p->upage = upage;
    p->kpage = kpage;

    p->thread = thread_current ();

    p->status = PAGE_STATUS_FRAME;
    p->origin = PAGE_STATUS_FRAME;

    p->swap_index = -1;

    p->file = NULL;
    p->ofs = 0;
    p->read_bytes = 0;
    p->zero_bytes = 0;
    p->writable = true;

    hash_insert (&thread_current ()->page_table, &p->elem);
    // printf("page_alloc_frame end\n");
}

void
page_alloc_zero (void *upage)
{
    // printf("page_alloc_zero\n");
    struct page *p = malloc(sizeof(struct page));
    p->upage = upage;
    p->kpage = NULL;

    p->thread = thread_current ();

    p->status = PAGE_STATUS_ZERO;
    p->origin = PAGE_STATUS_ZERO;

    p->swap_index = -1;

    p->file = NULL;
    p->ofs = 0;
    p->read_bytes = 0;
    p->zero_bytes = 0;
    p->writable = true;

    hash_insert (&thread_current ()->page_table, &p->elem);
    // printf("page_alloc_zero end\n");
}

void
page_alloc_file (void *upage, struct file *file, off_t ofs,
                 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
    // printf("page_alloc_file\n");
    struct page *p = malloc(sizeof(struct page));
    p->upage = upage;
    p->kpage = NULL;

    p->thread = thread_current ();

    p->status = PAGE_STATUS_FILE;
    p->origin = PAGE_STATUS_FILE;

    p->swap_index = -1;

    p->file = file;
    p->ofs = ofs;
    p->read_bytes = read_bytes;
    p->zero_bytes = zero_bytes;
    p->writable = writable;

    hash_insert (&thread_current ()->page_table, &p->elem);
    // printf("page_alloc_file end, writable: %d\n", p->writable);
}

// void
// page_free (struct hash *page_table, void *upage)
// {
//     struct page *p;
//     p = page_lookup (page_table, upage);

//     if (p == NULL)
//     {
//         // double free???
//         syscall_exit (-1);
//     }

//     switch (p->status)
//     {
//         case PAGE_STATUS_FRAME:
//             frame_free (p->kpage);
//             break;
//         case PAGE_STATUS_SWAP:
//             swap_free (p->swap_index);
//             break;
//         case PAGE_STATUS_ZERO:
//         case PAGE_STATUS_FILE:
//             // do nothing
//             break;
//         default:
//             // do nothing
//             break;
//     }

//     hash_delete (&thread_current ()->page_table, &p->elem);
//     free (p);
// }

struct page *
page_lookup (struct hash *page_table, void *upage)
{
    struct page p;
    struct hash_elem *e;
    p.upage = upage;
    e = hash_find (page_table, &p.elem);
    return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}

bool
page_load (void *fault_page)
{
    struct page *p = page_lookup (&thread_current ()->page_table, fault_page);
    if (p == NULL)
    {
        return false;
    }

    void *kpage = frame_alloc (PAL_USER, p->upage);
    if (kpage == NULL)
    {
        return false;
    }

    switch (p->status)
    {
        case PAGE_STATUS_ZERO:
            memset (kpage, 0, PGSIZE);
            break;
        case PAGE_STATUS_FILE:
        {
            bool lock_held = lock_held_by_current_thread (&file_lock);
            if (!lock_held)
            {
                lock_acquire (&file_lock);
            }
            if (file_read_at (p->file, kpage, p->read_bytes, p->ofs) != (int) p->read_bytes)
            {
                lock_release (&file_lock);
                frame_free (kpage);
                return false;
            }
            if (!lock_held)
            {
                lock_release (&file_lock);
            }
            memset (kpage + p->read_bytes, 0, p->zero_bytes);
            break;
        }
        case PAGE_STATUS_SWAP:
            swap_in (p->swap_index, kpage);
            break;
        case PAGE_STATUS_FRAME:
            // page shouldn't be here
            // as allocated address should not fault
        default:
            frame_free (kpage);
            return false;
    }

    struct thread* cur = thread_current ();
    // user page must be unmapped to pagedir
    if (!(pagedir_get_page (cur->pagedir, p->upage) == NULL
          && pagedir_set_page (cur->pagedir, p->upage, kpage, p->writable)))
    {
        frame_free (kpage);
        return false;
    }

    p->kpage = kpage;
    p->status = PAGE_STATUS_FRAME;
    return true;
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
    // printf("page_destructor\n");
    struct page *p = hash_entry (e, struct page, elem);
    switch (p->status)
    {
        case PAGE_STATUS_SWAP:
            swap_free (p->swap_index);
            break;
        case PAGE_STATUS_FRAME:
            // printf("page_destructor: frame\n");
            // if file-backed page, save back to file if dirty and writable
            if (p->origin == PAGE_STATUS_FILE && p->writable &&
                pagedir_is_dirty (p->thread->pagedir, p->upage))
            {
                bool lock_held = lock_held_by_current_thread (&file_lock);
                if (!lock_held)
                {
                    lock_acquire (&file_lock);
                }
                file_write_at (p->file, p->upage, p->read_bytes, p->ofs);
                if (!lock_held)
                {
                    lock_release (&file_lock);
                }
            }
            break;
        case PAGE_STATUS_FILE:
            // do nothing
        case PAGE_STATUS_ZERO:
            // not allocated; do nothing
        default:
            break;
    }
    free (p);
}
