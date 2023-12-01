#include "vm/frame.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"

static struct list frame_table;
static struct lock frame_lock;

struct frame *frame_evict(void);

void frame_init (void)
{
    list_init (&frame_table);
    lock_init (&frame_lock);
}

void *frame_alloc (enum palloc_flags flags, void *upage)
{
    // printf("frame_alloc %p\n", upage);
    struct frame *frame = NULL;
    void *kpage = palloc_get_page (flags);
    if (kpage == NULL)
    {
        // must return a used frame
        frame = frame_evict ();
    }
    else
    {
        frame = malloc (sizeof (struct frame));
    }

    frame->kpage = kpage;
    frame->upage = upage;
    frame->thread = thread_current ();

    lock_acquire (&frame_lock);
    list_push_back (&frame_table, &frame->elem);
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

struct frame* frame_evict(void)
{
    // do nothing for now
    return NULL;
}
