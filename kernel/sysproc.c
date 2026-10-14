
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
#include "include/signal.h"

extern int exec(char *path, char **argv);

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if (argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0 ||
      uargv == NULL) {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for (i = 0;; i++) {
    if (i >= NELEM(argv)) {
      goto bad;
    }
    if (fetchaddr(uargv + sizeof(uint64) * i, (uint64 *)&uarg) < 0) {
      goto bad;
    }
    if (uarg == 0) {
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if (argv[i] == 0)
      goto bad;
    if (fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

bad:
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_execve(void)
{
  uint64 uenvp, uenv;

  if (argaddr(2, &uenvp) < 0)
    return -1;

  if (uenvp == 0)
    return sys_exec();

  // Doesn't support environment variables now.
  if (fetchaddr(uenvp, &uenv) < 0 || uenv != 0)
    return -1;

  return sys_exec();
}

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_getppid(void)
{
  struct proc *p = myproc();
  uint64 result = 0;

  acquire(&p->lock);
  struct proc *parent = p->parent;
  if (parent != NULL)
    result = parent->pid;
  release(&p->lock);

  return result;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_clone(void)
{
  uint64 flags, child_stack;

  if (argaddr(0, &flags) < 0 || argaddr(1, &child_stack) < 0)
    return -1;

  if (flags != SIGCHLD) {
    // Doesn't support other clone flags now.
    return -1;
  }

  return fork_with_stack(child_stack);
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_wait4(void)
{
  int pid, options;
  uint64 status_addr, rusage_addr;

  if (argint(0, &pid) < 0 || argaddr(1, &status_addr) < 0 ||
      argint(2, &options) < 0 || argaddr(3, &rusage_addr) < 0)
    return -1;

  // Currently, resource usage reporting is unsupported.
  if (rusage_addr != 0)
    return -1;

  return wait4(pid, status_addr, options);
}

uint64
sys_sched_yield(void)
{
  yield();
  return 0;
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_brk(void)
{
  uint64 addr, dest;

  if (argaddr(0, &dest) < 0)
    return -1;
  addr = myproc()->sz;

  if (dest == 0)
    return addr;

  int64 delta = (int64)dest - addr;
  if (delta < XV6_INT_MIN || delta > XV6_INT_MAX)
    return -1;

  if (growproc((int)delta) < 0)
    return -1;

  return myproc()->sz;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
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

  if (argint(0, &pid) < 0)
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

// Get the current time.
uint64
sys_gettimeofday(void)
{
  uint64 tv_addr, tz_addr;
  struct timeval tv;

  if (argaddr(0, &tv_addr) < 0 || argaddr(1, &tz_addr) < 0)
    return -1;

  // Only support tz_addr == 0.
  if (tv_addr == 0 || tv_addr >= MAXUVA || sizeof(tv) > MAXUVA - tv_addr ||
      tz_addr != 0)
    return -1;

  // Using provided RISC-V time CSR (r_time).
  uint64 hticks = r_time();

  tv.tv_sec = hticks / TIMEBASE_FREQ;
  tv.tv_usec = (hticks % TIMEBASE_FREQ) * MICRO_SEC / TIMEBASE_FREQ;

  return copyout2(tv_addr, (char *)&tv, sizeof(tv));
}

// Suspend the current process for a specified duration.
uint64
sys_nanosleep(void)
{
  uint64 req_addr, rem_addr;
  struct timespec req;

  if (argaddr(0, &req_addr) < 0 || argaddr(1, &rem_addr) < 0)
    return -1;

  if (req_addr == 0 || req_addr >= MAXUVA || sizeof(req) > MAXUVA - req_addr)
    return -1;

  if (copyin2((char *)&req, req_addr, sizeof(req)) < 0)
    return -1;

  if (req.tv_nsec >= NANO_SEC)
    return -1;

  // Convert duration to hardware timer counts.
  // Round up fractional counts to avoid sleeping too little.
  uint64 frac_ticks = (req.tv_nsec * TIMEBASE_FREQ + NANO_SEC - 1) / NANO_SEC;

  // Check for arithmetic overflow.
  if (req.tv_sec > (XV6_LONG_MAX - frac_ticks) / TIMEBASE_FREQ)
    return -1;

  uint64 req_ticks = req.tv_sec * TIMEBASE_FREQ + frac_ticks;

  uint64 now = r_time();
  if (req_ticks > XV6_LONG_MAX - now)
    return -1;

  uint64 ddl_ticks = now + req_ticks;
  struct proc *p = myproc();
  int interrupted = 0;

  // Sleep until the deadline is reached.
  acquire(&tickslock);

  while (r_time() < ddl_ticks) {
    // Handle interruption by a killed process.
    if (p->killed) {
      interrupted = 1;
      break;
    }

    // Atomically release tickslock and sleep.
    // Reacquire tickslock after being awakened.
    sleep(&ticks, &tickslock);
  }

  release(&tickslock);

  if (interrupted) {
    if (rem_addr != 0) {
      now = r_time();

      uint64 rem_ticks = now < ddl_ticks ? ddl_ticks - now : 0;

      struct timespec rem;
      rem.tv_sec = rem_ticks / TIMEBASE_FREQ;
      rem.tv_nsec = (rem_ticks % TIMEBASE_FREQ) * NANO_SEC / TIMEBASE_FREQ;

      if (copyout2(rem_addr, (char *)&rem, sizeof(rem)) < 0)
        return -1;
    }
    return -1;
  }

  return 0;
}

uint64
sys_trace(void)
{
  int mask;
  if (argint(0, &mask) < 0) {
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

  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  if (copyout2(addr, (char *)&un, sizeof(un)) < 0) {
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

  if (argaddr(0, &addr) < 0 || argaddr(1, &length) < 0 ||
      argint(2, &prot) < 0 || argint(3, &flags) < 0 || argint(4, &fd) < 0 ||
      argaddr(5, &offset) < 0)
    return -1;

  // Check mapping parameters.
  if (length == 0 || length > MAXUVA)
    return -1;

  if (offset % PGSIZE != 0)
    return -1;

  if (flags != MAP_SHARED && flags != MAP_PRIVATE)
    return -1;

  if (prot == 0 || (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)))
    return -1;

  // Check the file descriptor and mapping range
  if (fd < 0 || fd >= NOFILE)
    return -1;

  struct file *f = p->ofile[fd];
  if (f == NULL || f->type != FD_ENTRY || !f->readable)
    return -1;

  if (flags == MAP_SHARED && !f->writable)
    return -1;

  // Only supports mappings within file size.
  if (offset > f->ep->file_size || length > f->ep->file_size - offset)
    return -1;

  // Choose a virtual address.
  uint64 size = PGROUNDUP(length);
  uint64 va;

  if (vma_find_free_addr(p, 0, size, &va) < 0)
    return -1;

  // Prevent overlapping or unaligned mappings.
  if (va % PGSIZE != 0 || va < PGROUNDUP(p->sz) || va > MAXUVA - size)
    return -1;

  // Lazy allocation.
  struct vma *v = vma_alloc(p);
  if (v == NULL)
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

  if (argaddr(0, &addr) < 0 || argaddr(1, &length) < 0)
    return -1;

  return mmap_unmap(p, addr, length);
}
