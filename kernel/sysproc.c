
#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/sbi.h"
#include "include/utsname.h"
#include "include/vm.h"
#include "include/fat32.h"
#include "include/mman.h"


extern int exec(char *path, char **argv);

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_brk(void)
{
  uint64 addr, dest;

  if(argaddr(0, &dest) < 0)
    return -1;
  addr = myproc()->sz;

  if(dest == 0)
    return addr;

  int64 delta = (int64)dest - addr;
  if(delta < XV6_INT_MIN || delta > XV6_INT_MAX)
    return -1;

  if(growproc((int)delta) < 0)
    return -1;

  return myproc()->sz;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}

uint64
sys_shutdown(void)
{
  sbi_shutdown();
  return 0;
}

uint64
sys_uname(void)
{
  uint64 addr;
  struct utsname un = {
    .sysname = "xv6",
    .nodename = "miniyuan",
    .release = "1.0",
    .version = "xv6-k210",
    .machine = "riscv64",
    .domainname = "localdomain",
  };

  if(argaddr(0, &addr) < 0) {
    return -1;
  }

  if(copyout2(addr, (char *)&un, sizeof(un)) < 0) {
    return -1;
  }

  return 0;
}

uint64
sys_mmap(void)
{
  uint64 addr, length, offset;
  int prot, flags, fd;
  struct proc *p = myproc();

  if(
    argaddr(0, &addr) < 0 ||
    argaddr(1, &length) < 0 ||
    argint(2, &prot) < 0 ||
    argint(3, &flags) < 0 ||
    argint(4, &fd) < 0 ||
    argaddr(5, &offset) < 0
  )
    return -1;

  // Check mapping parameters.
  if(length == 0 || length > MAXUVA)
    return -1;
  
  // This version only supports one mapped file.
  if(p->mmap_file != NULL)
    return -1;

  if(offset % PGSIZE != 0)
    return -1;

  if(flags != MAP_SHARED && flags != MAP_PRIVATE)
    return -1;

  if(
    prot == 0 ||
    (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
  )
    return -1;

  // Check the file descriptor and mapping range
  if(fd < 0 || fd >= NOFILE)
    return -1;

  struct file *f = p->ofile[fd];
  if(f == NULL || f->type != FD_ENTRY || !f->readable)
    return -1;

  if(flags == MAP_SHARED && !f->writable)
    return -1;

  // This version only supports mappings within file size.
  if(
    offset > f->ep->file_size ||
     length > f->ep->file_size - offset
  )
    return -1;

  uint64 size = PGROUNDUP(length);
  uint64 va = addr ? addr : MAXUVA - size; // default allocate from the top (MAXUVA)
  int mapped = 0;

  // Prevent overlapping or unaligned mappings.
  if(
    size == 0 ||
    size > MAXUVA ||
    va % PGSIZE != 0 ||
    va < PGROUNDUP(p->sz) ||
    va > MAXUVA - size
  )
    return -1;

  for(uint64 i = 0; i < size; i += PGSIZE) {
    pte_t *upte = walk(p->pagetable, va + i, 0);
    pte_t *kpte = walk(p->kpagetable, va + i, 0);

    if((upte && (*upte & PTE_V)) ||
       (kpte && (*kpte & PTE_V)))
      return -1;
  }

  // Translate mmap protection flags into PTE permissions.
  int perm = 0;
  if(prot & PROT_READ)
    perm |= PTE_R;
  if(prot & PROT_WRITE)
    perm |= PTE_R | PTE_W;
  if(prot & PROT_EXEC)
    perm |= PTE_X;

  // Eagerly allocate.
  for(uint64 i = 0; i < size; i += PGSIZE) {
    char *mem = kalloc();
    if(mem == 0)
      goto bad;

    memset(mem, 0, PGSIZE);
    uint64 n = length - i;
    if(n > PGSIZE)
      n = PGSIZE;

    elock(f->ep);
    int r = eread(f->ep, 0, (uint64)mem, offset + i, n);
    eunlock(f->ep);

    if(r != n) {
      kfree(mem);
      goto bad;
    }

    // Map into user page table.
    if(mappages(p->pagetable, va + i, PGSIZE, (uint64)mem, perm | PTE_U) < 0) {
      kfree(mem);
      goto bad;
    }

    // Map into kernel page table.
    if(mappages(p->kpagetable, va + i, PGSIZE, (uint64)mem, PTE_R | PTE_W) < 0) {
      vmunmap(p->pagetable, va + i, 1, 1);
      goto bad;
    }

    mapped++;
  }

  p->mmap_addr = va;
  p->mmap_length = length;
  p->mmap_offset = offset;
  p->mmap_flags = flags;
  p->mmap_file = filedup(f);

  // Synchronize page-table updates with address translation.
  sfence_vma();
  return va;

bad:
  // Roll back.
  if(mapped > 0) {
    vmunmap(p->kpagetable, va, mapped, 0);
    vmunmap(p->pagetable, va, mapped, 1);
  }

  sfence_vma();
  return -1;
}

uint64
sys_munmap(void)
{
  uint64 addr, length;
  struct proc *p = myproc();

  if(
    argaddr(0, &addr) < 0 ||
    argaddr(1, &length) < 0
  )
    return -1;

  // Only support unmapping the entire recorded region.
  if(
    p->mmap_file == 0 ||
    addr != p->mmap_addr ||
    length != p->mmap_length
  )
    return -1;

  struct file *f = p->mmap_file;

  // Write back MAP_SHARED pages.
  if(p->mmap_flags == MAP_SHARED) {
    for(uint64 i = 0; i < length; i += PGSIZE) {
      uint64 n = length - i;
      if(n > PGSIZE)
        n = PGSIZE;

      uint64 pa = walkaddr(p->pagetable, addr + i);
      if(pa == 0)
        return -1;

      elock(f->ep);
      int r = ewrite(f->ep, 0, pa, p->mmap_offset + i, n);
      eunlock(f->ep);

      if(r != n)
        return -1;
    }
  }

  // Remove mappings.
  uint64 npages = PGROUNDUP(length) / PGSIZE;
  vmunmap(p->kpagetable, addr, npages, 0);
  vmunmap(p->pagetable, addr, npages, 1);

  // Synchronize page-table updates with address translation.
  sfence_vma();

  // Release the reference acquired by filedup().
  fileclose(f);

  p->mmap_addr   = 0;
  p->mmap_length = 0;
  p->mmap_offset = 0;
  p->mmap_flags  = 0;
  p->mmap_file   = NULL;

  return 0;
}
