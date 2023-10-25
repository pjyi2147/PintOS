#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
syscall_halt (void)
{
  shutdown_power_off();
}

void
syscall_exit (int status)
{
}

tid_t
syscall_exec (const char *cmd_line)
{
}

int
syscall_wait (tid_t pid UNUSED)
{
  process_wait(pid);
}

bool
syscall_create (const char *file, unsigned initial_size)
{
}

bool
syscall_remove (const char *file)
{
}

int
syscall_open (const char *file)
{
}

int
syscall_filesize (int fd)
{
}

int
syscall_read (int fd, void *buffer, unsigned size)
{
}

int
syscall_write (int fd, const void *buffer, unsigned size)
{
}

void
syscall_seek (int fd, unsigned position)
{
}

unsigned
syscall_tell (int fd)
{
}

void
syscall_close (int fd)
{
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
