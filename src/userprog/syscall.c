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
get_argument (void *esp, void **arg, int count)
{
  int i;
  for (i = 0; i < count; i++)
  {
    if (!is_user_vaddr(esp + 4 * i))
      syscall_exit(-1);

    arg[i] = *(void **)(esp + 4 * i);
  }
}

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
  if (!is_user_vaddr (cmd_line))
  {
    syscall_exit(-1);
  }
  char *fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
  {
    syscall_exit(-1);
  }
  strlcpy(fn_copy, cmd_line, PGSIZE);
  return process_execute (fn_copy);
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
  if (fd >= FILE_MAX || t->file_array[fd] != NULL)
  {
    return -1;
  }

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
  if (!is_user_vaddr(f->esp))
  {
    syscall_exit(-1);
  }

  // printf("system call! %d\n", *(int *)f->esp);

  void *arg[3];

  switch (*(int *)f->esp)
  {
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      get_argument(f->esp + 4, arg, 1);
      syscall_exit((int)arg[0]);
      break;
    case SYS_EXEC:
      get_argument(f->esp + 4, arg, 1);
      if (syscall_exec((char *)arg[0]) == -1)
      {
        syscall_exit(-1);
      }
      break;
    case SYS_WAIT:
      get_argument(f->esp + 4, arg, 1);
      f->eax = syscall_wait((tid_t)arg[0]);
      break;
    case SYS_CREATE:
      get_argument(f->esp + 4, arg, 2);
      f->eax = syscall_create((char *)arg[0], (unsigned)arg[1]);
      break;
    case SYS_REMOVE:
      get_argument(f->esp + 4, arg, 1);
      f->eax = syscall_remove((char *)arg[0]);
      break;
    case SYS_OPEN:
      get_argument(f->esp + 4, arg, 1);
      f->eax = syscall_open((char *)arg[0]);
      break;
    case SYS_FILESIZE:
      get_argument(f->esp + 4, arg, 1);
      f->eax = syscall_filesize((int)arg[0]);
      break;
    case SYS_READ:
      get_argument(f->esp + 4, arg, 3);
      f->eax = syscall_read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_WRITE:
      get_argument(f->esp + 4, arg, 3);
      f->eax = syscall_write((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_SEEK:
      get_argument(f->esp + 4, arg, 2);
      syscall_seek((int)arg[0], (unsigned)arg[1]);
      break;
    case SYS_TELL:
      get_argument(f->esp + 4, arg, 1);
      f->eax = syscall_tell((int)arg[0]);
      break;
    case SYS_CLOSE:
      get_argument(f->esp + 4, arg, 1);
      syscall_close((int)arg[0]);
      break;
  }
}
