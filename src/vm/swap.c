#include "vm/swap.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/syscall.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct bitmap *swap_table;
static struct block *swap_block;
static struct lock swap_lock;

void swap_table_init(void)
{
  swap_block = block_get_role (BLOCK_SWAP);
  swap_table = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE);

  bitmap_set_all(swap_table, false);
  lock_init(&swap_lock);
}

void swap_in(unsigned swap_index, void *kva)
{
  lock_acquire (&swap_lock);
  if (swap_index > bitmap_size (swap_table))
  {
    syscall_exit (-1);
  }

  if (bitmap_test (swap_table, swap_index) == false)
  {
    syscall_exit (-1);
  }

  bitmap_set (swap_table, swap_index, false);
  lock_release (&swap_lock);

  for (int i = 0; i < SECTORS_PER_PAGE; i++)
  {
    block_read (swap_block, swap_index * SECTORS_PER_PAGE + i, kva + i * BLOCK_SECTOR_SIZE);
  }
}

int swap_out(void *kva)
{
  lock_acquire(&swap_lock);
  int swap_index = bitmap_scan_and_flip (swap_table, 0, 1, false);
  lock_release(&swap_lock);

  for (int i = 0; i < SECTORS_PER_PAGE; i++)
  {
    block_write (swap_block, swap_index * SECTORS_PER_PAGE + i, kva + i * BLOCK_SECTOR_SIZE);
  }
  return swap_index;
}

void swap_free(unsigned swap_index)
{
  lock_acquire (&swap_lock);
  if (swap_index > bitmap_size (swap_table))
  {
    syscall_exit (-1);
  }

  if (bitmap_test (swap_table, swap_index) == false)
  {
    syscall_exit (-1);
  }

  bitmap_set (swap_table, swap_index, false);
  lock_release (&swap_lock);
}
