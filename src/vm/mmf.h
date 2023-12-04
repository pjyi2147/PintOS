#ifndef VM_MMF_H
#define VM_MMF_H

#include <list.h>
#include "filesys/file.h"

struct mmf
{
    int id;
    struct list_elem elem;
    struct file *file;
    void *upage;
};

struct mmf *mmf_init(int id, struct file *file, void *upage);
struct mmf *mmf_get(int id);
void mmf_cleanup (void);

#endif /* VM_MMF_H */
