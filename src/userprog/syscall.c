#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/stdio.h"

static void syscall_handler (struct intr_frame *);

struct lock file_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

void
syscall_halt (void)
{
  shutdown_power_off();
}

void
syscall_exit (int status)
{
  struct thread *t = thread_current();
  t->exit_status = status;
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit();
}

tid_t
syscall_exec (const char *cmd_line)
{
  is_user_vaddr (cmd_line);
  return process_execute (cmd_line);
}

int
syscall_wait (tid_t pid UNUSED)
{
  process_wait(pid);
}

bool
syscall_create (const char *file, unsigned initial_size)
{
  if (!is_user_vaddr(file))
  {
    syscall_exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool
syscall_remove (const char *file)
{
  if (!is_user_vaddr(file))
  {
    syscall_exit(-1);
  }
  return filesys_remove(file);
}

int
syscall_open (const char *file)
{
  if (!is_user_vaddr(file))
  {
    syscall_exit(-1);
  }

  struct file *f = filesys_open(file);
  if (f == NULL)
  {
    return -1;
  }

  struct thread *t = thread_current();
  int fd = t->min_fd;
  ASSERT(fd < FILE_MAX);
  ASSERT(t->file_array[fd] == NULL);
  t->file_array[fd] = f;

  bool is_min_fd = false;
  for (int i = 2; i < FILE_MAX; i++)
  {
    if (t->file_array[i] == NULL)
    {
      t->min_fd = i;
      is_min_fd = true;
      break;
    }
  }

  if (!is_min_fd)
  {
    t->min_fd = FILE_MAX;
    return -1;
  }

  return fd;
}

int
syscall_filesize (int fd)
{
  struct thread *t = thread_current();
  struct file *f = t->file_array[fd];
  if (f == NULL)
  {
    return -1;
  }
  return file_length(f);
}

int
syscall_read (int fd, void *buffer, unsigned size)
{
  // Check buffer
  if (!is_user_vaddr(buffer))
  {
    syscall_exit(-1);
  }
  // STDIN
  if (fd == 0)
  {
    for (int i = 0; i < size; i++)
    {
      ((char *)buffer)[i] = input_getc();
    }
    return size;
  }

  // STDOUT
  else if (fd == 1)
  {
    return -1;
  }

  struct thread *t = thread_current();
  struct file *f = t->file_array[fd];
  if (f == NULL)
  {
    return -1;
  }

  lock_acquire(&file_lock);
  int ret = file_read(f, buffer, size);
  lock_release(&file_lock);
  return ret;
}

int
syscall_write (int fd, const void *buffer, unsigned size)
{
  // Check buffer
  if (!is_user_vaddr(buffer))
  {
    syscall_exit(-1);
  }
  // STDIN
  if (fd == 0)
  {
    return -1;
  }
  // STDOUT
  else if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }

  struct thread *t = thread_current();
  struct file *f = t->file_array[fd];
  if (f == NULL)
  {
    return -1;
  }

  lock_acquire(&file_lock);
  int ret = file_write(f, buffer, size);
  lock_release(&file_lock);
  return ret;
}

void
syscall_seek (int fd, unsigned position)
{
  struct thread *t = thread_current();
  struct file *f = t->file_array[fd];
  if (f == NULL)
  {
    return;
  }
  file_seek(f, position);
}

unsigned
syscall_tell (int fd)
{
  struct thread *t = thread_current();
  struct file *f = t->file_array[fd];
  if (f == NULL)
  {
    return -1;
  }
  return file_tell(f);
}

void
syscall_close (int fd)
{
  struct thread *t = thread_current();
  struct file *f = t->file_array[fd];
  if (f == NULL)
  {
    return;
  }

  file_close(f);
  t->file_array[fd] = NULL;
  if (fd < t->min_fd)
  {
    t->min_fd = fd;
  }
}

static void
syscall_handler (struct intr_frame *f)
{
  switch (f->eax)
  {
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      syscall_exit(f->edi);
      break;
    case SYS_EXEC:
      if (syscall_exec(f->edi) == -1)
      {
        syscall_exit(-1);
      }
      break;
    case SYS_WAIT:
      f->eax = syscall_wait(f->edi);
      break;
    case SYS_CREATE:
      f->eax = syscall_create(f->edi, f->esi);
      break;
    case SYS_REMOVE:
      f->eax = syscall_remove(f->edi);
      break;
    case SYS_OPEN:
      f->eax = syscall_open(f->edi);
      break;
    case SYS_FILESIZE:
      f->eax = syscall_filesize(f->edi);
      break;
    case SYS_READ:
      f->eax = syscall_read(f->edi, f->esi, f->edx);
      break;
    case SYS_WRITE:
      f->eax = syscall_write(f->edi, f->esi, f->edx);
      break;
    case SYS_SEEK:
      syscall_seek(f->edi, f->esi);
      break;
    case SYS_TELL:
      f->eax = syscall_tell(f->edi);
      break;
    case SYS_CLOSE:
      syscall_close(f->edi);
      break;
  }
}

void
exit (int stat)
{
  printf ("%s: exit(%d)\n", thread_name(), stat);
  thread_exit ();
}
