#include "vm/frame.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct list frame_table;
static struct lock frame_lock;
struct list_elem* first_frame_elem;
extern struct lock file_lock;

void frame_evict(void);

void frame_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_lock);
    first_frame_elem = NULL;
}

void *frame_alloc (enum palloc_flags flags, void *upage)
{
    // printf("frame_alloc %p\n", upage);
    struct frame *frame = NULL;
    void *kpage = palloc_get_page (flags);
    if (kpage == NULL)
    {
        // must return a used frame
        frame_evict ();
    }
    frame = malloc (sizeof (struct frame));

    if (frame == NULL)
    {
        // printf("frame_alloc: frame_evict failed\n");
        return NULL;
    }

    frame->kpage = kpage;
    frame->upage = upage;
    frame->thread = thread_current ();

    lock_acquire (&frame_lock);
    printf("frame_alloc push_back\n");
    list_push_back (&frame_table, &frame->elem);
    printf("frame_alloc push_back end\n");
    lock_release (&frame_lock);

    return kpage;
}

void frame_free (void *kpage)
{
    struct list_elem *e;
    struct frame *frame;
    lock_acquire (&frame_lock);
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_begin (&frame_table))
    {
        frame = list_entry (e, struct frame, elem);
        if (frame->kpage == kpage)
        {
            list_remove (e);
            free (frame);
            break;
        }
    }
    lock_release (&frame_lock);
    palloc_free_page (kpage);
}

void
frame_table_cleanup(void)
{
    // printf("frame_table_cleanup\n");
    struct list_elem *e;
    struct frame *frame;
    lock_acquire(&frame_lock);
    for (e = list_begin(&frame_table); e != list_end(&frame_table);)
    {
        frame = list_entry (e, struct frame, elem);
        if (frame->thread == thread_current ())
        {
            struct list_elem *next = list_next (e);
            list_remove (e);
            e = next;
            free (frame);
        }
        else
        {
            e = list_next (e);
        }
    }
    lock_release (&frame_lock);
}

void frame_evict(void)
{
    // printf("frame_evict\n");
    struct list_elem *e;
    struct frame *frame;
    struct thread *cur = thread_current ();
    lock_acquire (&frame_lock);
    if (first_frame_elem == NULL || first_frame_elem == list_end (&frame_table))
    {
        first_frame_elem = list_begin (&frame_table);
    }
    e = first_frame_elem;

    while (e != list_end (&frame_table))
    {
        frame = list_entry (e, struct frame, elem);
        // update first_frame_elem
        if (list_next(e) == list_end(&frame_table))
        {
            first_frame_elem = list_begin (&frame_table);
        }
        else
        {
            first_frame_elem = list_next (e);
        }

        if (pagedir_is_accessed (cur->pagedir, frame->upage))
        {
            pagedir_set_accessed (cur->pagedir, frame->upage, false);
            e = first_frame_elem;
        }
        // evict the page here
        else
        {
            struct page *p = page_lookup (&cur->page_table, frame->upage);
            if (p == NULL)
            {
                lock_release (&frame_lock);
                syscall_exit (-2384);
            }
            ASSERT (p->kpage == frame->kpage);

            switch (p->origin)
            {
                case PAGE_STATUS_FRAME:
                case PAGE_STATUS_ZERO:
                    p->swap_index = swap_out (frame->kpage);
                    p->status = PAGE_STATUS_SWAP;
                    break;
                case PAGE_STATUS_FILE:
                {
                    if (p->writable && pagedir_is_dirty (cur->pagedir, frame->upage))
                    {
                        bool lock_held = lock_held_by_current_thread (&file_lock);
                        if (!lock_held)
                        {
                            lock_acquire (&file_lock);
                        }
                        if (file_write_at (p->file, frame->kpage, p->read_bytes, p->ofs) != (int) p->read_bytes)
                        {
                            if (!lock_held)
                            {
                                lock_release (&file_lock);
                            }
                            lock_release (&frame_lock);
                            syscall_exit (-1);
                        }
                        if (!lock_held)
                        {
                            lock_release (&file_lock);
                        }
                        pagedir_set_dirty (cur->pagedir, frame->upage, false);
                    }
                    p->status = PAGE_STATUS_FILE;
                    break;
                }
                case PAGE_STATUS_SWAP:
                    // do nothing; but should not happen
                    break;
            }
            list_remove (e);
            free (frame);
            lock_release (&frame_lock);
        }
    }
    lock_release (&frame_lock);
    // something went wrong
    syscall_exit (-1234);
}
