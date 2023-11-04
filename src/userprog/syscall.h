#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define STACK_BOTTOM 0x8048000

#include <stdbool.h>
#include "threads/thread.h"

void get_stack_args (void *esp, void **arg, int count);
bool validate_addr (void *addr);
void syscall_halt (void);
void syscall_exit (int status);
tid_t syscall_exec (const char *cmd_line);
int syscall_wait (tid_t pid);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell (int fd);
void syscall_close (int fd);

void syscall_init (void);

#endif /* userprog/syscall.h */
