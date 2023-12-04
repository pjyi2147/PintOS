#ifndef VM_MMF_H
#define VM_MMF_H

#include <list.h>
#include "filesys/file.h"

struct mmf
{
   int id;
   struct file *file;
   struct list_elem elem;

   void *upage;
};

struct mmf *mmf_init(int id, struct file *file, void *upage);
struct mmf *mmf_get(int id);

#endif /* VM_MMF_H */
