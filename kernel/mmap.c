#include "include/mmap.h"
#include "include/fat32.h"
#include "include/kalloc.h"
#include "include/memlayout.h"
#include "include/mman.h"
#include "include/param.h"
#include "include/printf.h"
#include "include/proc.h"
#include "include/riscv.h"
#include "include/string.h"
#include "include/types.h"
#include "include/vm.h"

// Initialize the VMA table.
void
vma_init(struct proc *p) {
    memset(p->vmas, 0, sizeof(p->vmas));
}

// Allocate an unused vma entry.
struct vma *
vma_alloc(struct proc *p) {
    for (int i = 0; i < NVMA; i++) {
        struct vma *v = &p->vmas[i];

        if (!v->used) {
            memset(v, 0, sizeof(*v));
            v->used = 1;
            return v;
        }
    }

    return NULL;
}

// Find the vma *v containing the virtual address va.
// VMA v covers [v.addr, v.addr + PGROUNDUP(v.length)).
struct vma *
vma_find(struct proc *p, uint64 va) {
    for (int i = 0; i < NVMA; i++) {
        struct vma *v = &p->vmas[i];

        if (!v->used)
            continue;

        if (va >= v->addr && va < v->addr + PGROUNDUP(v->length)) {
            return v;
        }
    }

    return NULL;
}

// Find a VMA overlapping the va range [start, end).
// With lazy allocation, mapping conflicts must be detected using VMAs
// rather than PTEs, since reserved pages may not be mapped yet.
struct vma *
vma_find_overlap(struct proc *p, uint64 start, uint64 end) {
    for (int i = 0; i < NVMA; i++) {
        struct vma *v = &p->vmas[i];

        if (!v->used)
            continue;

        uint64 v_end = v->addr + PGROUNDUP(v->length);

        if (start < v_end && v->addr < end)
            return v;
    }

    return NULL;
}

// Find a free virtual address range for the given size.
// The size must be page-aligned.
// The starting va is stored in *va.
int
vma_find_free_addr(struct proc *p, uint64 addr, uint64 size, uint64 *va) {
    if (va == NULL || size == 0 || size % PGSIZE != 0)
        return -1;

    if (
        MMAPMIN >= MMAPBASE ||
        MMAPBASE > MAXUVA ||
        MMAPMIN % PGSIZE != 0 ||
        MMAPBASE % PGSIZE != 0)
        return -1;

    // User-specified address.
    if (addr != 0) {
        if (
            addr % PGSIZE != 0 ||
            addr < MMAPMIN ||
            addr >= MMAPBASE ||
            size > MMAPBASE - addr)
            return -1;

        if (vma_find_overlap(p, addr, addr + size))
            return -1;

        *va = addr;
        return 0;
    }

    // Otherwise autmatically find a free area.
    uint64 end = MMAPBASE;

    while (end >= MMAPMIN && size <= end - MMAPMIN) {
        uint64 start = end - size;

        struct vma *v = vma_find_overlap(p, start, end);

        if (v == NULL) {
            *va = start;
            return 0;
        }

        end = v->addr;
    }

    return -1;
}

// Copy VMAs and resident pages from p to np.
// Roll back all copied mappings on failure.
int
vma_copy(struct proc *p, struct proc *np)
{
  // First copy resident pages.
  // Because page copy may fail.
  for (int i = 0; i < NVMA; i++) {
    struct vma *v = &p->vmas[i];

    if (!v->used)
      continue;

    uint64 start = PGROUNDDOWN(v->addr);
    uint64 end = start + PGROUNDUP(v->length);
    
    if (uvmcopy_range(p->pagetable, np->pagetable,
                  np->kpagetable, start, end) < 0)
      goto bad;
  }

  // Then copy metadata and acquire file references.
  for (int i = 0; i < NVMA; i++) {
    if (!p->vmas[i].used)
        continue;

    np->vmas[i] = p->vmas[i];

    if (np->vmas[i].file)
      np->vmas[i].file = filedup(np->vmas[i].file);
  }

  return 0;

bad:
  for (int i = 0; i < NVMA; i++) {
    struct vma *v = &p->vmas[i];

    if (!v->used)
      continue;

    uint64 start = PGROUNDDOWN(v->addr);
    uint64 end = start + PGROUNDUP(v->length);

    vmunmap_sparse(np->kpagetable, start, end, 0);
    vmunmap_sparse(np->pagetable, start, end, 1);
  }

  return -1;
}

// Unmap all VMAs and release their file references.
// Continue cleanup even if file write-back fails.
int
vma_unmap_all(struct proc *p)
{
  int result = 0;

  for (int i = 0; i < NVMA; i++) {
    struct vma *v = &p->vmas[i];

    if (!v->used)
      continue;
    
    if (mmap_unmap(p, v->addr, v->length) < 0)
      result = -1;
  }

  return result;
}

// Handle a page fault within a lazily allocated mmap region.
int
mmap_fault(struct proc *p, uint64 fault_va, int cause) {
    struct vma *v = vma_find(p, fault_va);
    if (v == NULL)
        return -1;

    struct file *f = v->file;
    if (f == NULL)
        return -1;

    // Check access permissions.
    if (cause == 12 && !(v->prot & PROT_EXEC)) // Instruction Page Fault
        return -1;
    if (cause == 13 && !(v->prot & PROT_READ)) // Load Page Fault
        return -1;
    if (cause == 15 && !(v->prot & PROT_WRITE)) // Store Page Fault
        return -1;
    if (cause != 12 && cause != 13 && cause != 15)
        return -1;

    uint64 va = PGROUNDDOWN(fault_va);

    // A page fault may occur even if the page is already mapped,
    // due to insufficient access permissions rather than a missing page.
    // Lazy allocation should only handle unmapped pages.
    // Check both page tables to avoid duplicate or inconsistent mappings.
    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte != NULL && (*pte & PTE_V))
        return -1;

    pte = walk(p->kpagetable, va, 0);
    if (pte != NULL && (*pte & PTE_V))
        return -1;

    char *mem = kalloc();
    if (mem == NULL)
        return -1;

    memset(mem, 0, PGSIZE);
    uint64 vma_off = va - v->addr;
    uint64 n = v->length - vma_off;
    if (n > PGSIZE)
        n = PGSIZE;

    elock(f->ep);
    int r = eread(f->ep, 0, (uint64)mem, v->offset + vma_off, n);
    eunlock(f->ep);

    if (r != (int)n) {
        kfree(mem);
        return -1;
    }

    // Translate mmap protection flags into PTE permissions.
    int perm = 0;
    if (v->prot & PROT_READ)
        perm |= PTE_R;
    if (v->prot & PROT_WRITE)
        perm |= PTE_R | PTE_W;
    if (v->prot & PROT_EXEC)
        perm |= PTE_X;

    // Map into user page table.
    if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm | PTE_U) < 0) {
        kfree(mem);
        return -1;
    }

    // Map into kernel page table.
    if (mappages(p->kpagetable, va, PGSIZE, (uint64)mem, PTE_R | PTE_W) < 0) {
        vmunmap(p->pagetable, va, 1, 1);
        return -1;
    }

    // Synchronize page-table updates with address translation.
    sfence_vma();
    return 0;
}

// Unmap an entire VMA starting at addr.
// Partial unmapping is not supported.
// Returns 0 on success, or -1 on invalid arguments or write-back failure.
int
mmap_unmap(struct proc *p, uint64 addr, uint64 length) {
    struct vma *v = vma_find(p, addr);

    // Only complete VMA unmapping for now.
    if (
        v == NULL ||
        addr != v->addr ||
        length == 0 ||
        length > MAXUVA ||
        PGROUNDUP(length) != PGROUNDUP(v->length))
        return -1;

    int result = 0;
    uint64 size = PGROUNDUP(v->length);

    // Write back MAP_SHARED pages.
    if (v->flags & MAP_SHARED) {
        for (uint64 off = 0; off < size; off += PGSIZE) {
            uint64 va = v->addr + off;
            pte_t *pte = walk(p->pagetable, va, 0);

            if (pte == NULL || !(*pte & PTE_V))
                continue;

            uint64 pa = PTE2PA(*pte);
            uint64 n = v->length - off;

            if (n > PGSIZE)
                n = PGSIZE;

            elock(v->file->ep);
            int r = ewrite(v->file->ep, 0, pa, v->offset + off, n);
            eunlock(v->file->ep);

            if (r != (int)n)
                result = -1;
        }
    }

    // Remove resident pages from both page tables.
    for (uint64 off = 0; off < size; off += PGSIZE) {
        uint64 va = v->addr + off;

        pte_t *upte = walk(p->pagetable, va, 0);
        pte_t *kpte = walk(p->kpagetable, va, 0);

        if (upte != NULL && (*upte & PTE_V)) {
            if (kpte == NULL || !(*kpte & PTE_V))
                panic("mmap_unmap: inconsistent page tables");

            // Kernel mapping does not own the physical page.
            vmunmap(p->kpagetable, va, 1, 0);

            // User mapping releases the physical page.
            vmunmap(p->pagetable, va, 1, 1);
        } else if (kpte != NULL && (*kpte & PTE_V)) {
            panic("mmap_unmap: orphan kernel mapping");
        }
    }

    sfence_vma();

    // Release file reference.
    fileclose(v->file);
    // Clear the VMA metadata.
    memset(v, 0, sizeof(*v));

    return result;
}
