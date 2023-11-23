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

// project 2
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);

struct lock file_lock;

void
get_stack_args (void *esp, void **arg, int count)
{
  int i;
  for (i = 0; i < count; i++)
  {
    if (!is_user_vaddr(esp + sizeof(void *) * i))
      syscall_exit(-1);

    arg[i] = *(void **)(esp + sizeof(void *) * i);
  }
}

bool validate_addr (void *addr)
{
  if (addr >= (void *) STACK_BOTTOM && addr < PHYS_BASE && addr != 0)
    return true;

  return false;
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
  sema_up(&t->sema_load);
  sema_up(&t->sema_wait);

  thread_exit();
}

tid_t
syscall_exec (const char *cmd_line)
{
  return process_execute(cmd_line);
}

int
syscall_wait (tid_t pid)
{
  return process_wait (pid);
}

bool
syscall_create (const char *file, unsigned initial_size)
{
  if (!is_user_vaddr(file) || file == NULL)
  {
    syscall_exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool
syscall_remove (const char *file)
{
  if (!is_user_vaddr(file) || file == NULL)
  {
    syscall_exit(-1);
  }
  return filesys_remove(file);
}

int
syscall_open (const char *file)
{
  if (!is_user_vaddr(file) || file == NULL)
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
  if (fd < 0 || fd >= FILE_MAX)
  {
    return -1;
  }
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
  if (fd < 0 || fd >= FILE_MAX)
  {
    return -1;
  }
  // STDIN
  if (fd == 0)
  {
    for (unsigned i = 0; i < size; i++)
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
  if (fd < 0 || fd >= FILE_MAX)
  {
    return -1;
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
  if (fd < 0 || fd >= FILE_MAX)
  {
    return;
  }
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
  if (fd < 0 || fd >= FILE_MAX)
  {
    return 0;
  }
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
  if (fd < 0 || fd >= FILE_MAX)
  {
    return;
  }
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
  if (validate_addr(f->esp) == false)
    syscall_exit(-1);

  // printf("system call! %d\n", *(int *)f->esp);

  void *arg[3];

  switch (*(int *)f->esp)
  {
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      get_stack_args(f->esp + 4, arg, 1);
      syscall_exit((int)arg[0]);
      break;
    case SYS_EXEC:
      get_stack_args(f->esp + 4, arg, 1);
      int ret = syscall_exec((char *)arg[0]);
      if (ret == TID_ERROR)
      {
        syscall_exit(-1);
      }
      if (ret == -2)
      {
        ret = -1;
      }
      f->eax = ret;
      break;
    case SYS_WAIT:
      get_stack_args(f->esp + 4, arg, 1);
      f->eax = syscall_wait((int)arg[0]);
      break;
    case SYS_CREATE:
      get_stack_args(f->esp + 4, arg, 2);
      f->eax = syscall_create((char *)arg[0], (unsigned)arg[1]);
      break;
    case SYS_REMOVE:
      get_stack_args(f->esp + 4, arg, 1);
      f->eax = syscall_remove((char *)arg[0]);
      break;
    case SYS_OPEN:
      get_stack_args(f->esp + 4, arg, 1);
      f->eax = syscall_open((char *)arg[0]);
      break;
    case SYS_FILESIZE:
      get_stack_args(f->esp + 4, arg, 1);
      f->eax = syscall_filesize((int)arg[0]);
      break;
    case SYS_READ:
      get_stack_args(f->esp + 4, arg, 3);
      f->eax = syscall_read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_WRITE:
      get_stack_args(f->esp + 4, arg, 3);
      f->eax = syscall_write((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
      break;
    case SYS_SEEK:
      get_stack_args(f->esp + 4, arg, 2);
      syscall_seek((int)arg[0], (unsigned)arg[1]);
      break;
    case SYS_TELL:
      get_stack_args(f->esp + 4, arg, 1);
      f->eax = syscall_tell((int)arg[0]);
      break;
    case SYS_CLOSE:
      get_stack_args(f->esp + 4, arg, 1);
      syscall_close((int)arg[0]);
      break;
    default:
      syscall_exit(-1);
      break;
  }
}
