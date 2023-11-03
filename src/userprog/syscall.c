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
syscall_exit (int status)
{
  printf ("(%s): exit(%d)\n", thread_current()->name, status);
  thread_exit ();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  printf ("system call! %d \n", f->eax);
  switch (f->eax)
  {
  case SYS_EXIT:
    syscall_exit(f->edi);
    break;

  default:
    break;
  }
  thread_exit ();
}
