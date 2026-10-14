
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
#include "include/mmap.h"


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

  // Only supports mappings within file size.
  if(
    offset > f->ep->file_size ||
    length > f->ep->file_size - offset
  )
    return -1;

  // Choose a virtual address.
  uint64 size = PGROUNDUP(length);
  uint64 va;

  if(addr == 0 && vma_find_free_addr(p, 0, size, &va) < 0)
    return -1;

  // Prevent overlapping or unaligned mappings.
  if(
    va % PGSIZE != 0 ||
    va < PGROUNDUP(p->sz) ||
    va > MAXUVA - size
  )
    return -1;

  // Lazy allocation.
  struct vma *v = vma_alloc(p);
  if(v == NULL)
    return -1;

  v->addr = va;
  v->length = length;
  v->offset = offset;
  v->prot = prot;
  v->flags = flags;
  v->file = filedup(f);

  return va;
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

  return mmap_unmap(p, addr, length);
}
