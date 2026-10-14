#ifndef __MMAP_H
#define __MMAP_H

#include "types.h"

#define MMAPBASE 0x70000000UL // the highest address for mmap
#define MMAPMIN  0x60000000UL // the lowest address for mmap

struct           proc;
struct           vma;

void             vma_init(struct proc *p);
struct vma *     vma_alloc(struct proc *p);
struct vma *     vma_find(struct proc *p, uint64 va);
int              vma_find_free_addr(struct proc *p, uint64 addr, uint64 size, uint64 *va);
int              vma_copy(struct proc *p, struct proc *np);
int              vma_unmap_all(struct proc *p);
int              mmap_fault(struct proc *p, uint64 fault_va, int cause);
int              mmap_unmap(struct proc *p, uint64 addr, uint64 length);

#endif