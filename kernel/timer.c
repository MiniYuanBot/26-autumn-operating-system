// Timer Interrupt handler


#include "include/types.h"
#include "include/param.h"
#include "include/riscv.h"
#include "include/sbi.h"
#include "include/spinlock.h"
#include "include/timer.h"
#include "include/printf.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/vm.h"

struct spinlock tickslock;
uint ticks;

void timerinit() {
    initlock(&tickslock, "time");
    #ifdef DEBUG
    printf("timerinit\n");
    #endif
}

void
set_next_timeout() {
    // There is a very strange bug,
    // if comment the `printf` line below
    // the timer will not work.

    // this bug seems to disappear automatically
    // printf("");
    sbi_set_timer(r_time() + INTERVAL);
}

void
timer_tick() {
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
    set_next_timeout();
}

uint64
sys_times(void) {
    uint64 addr;
    struct tms t;
    uint64 ret;

    if (argaddr(0, &addr) < 0)
        return -1;

    acquire(&tickslock);
    t.tms_utime = t.tms_stime = t.tms_cutime = t.tms_cstime = ticks;
    ret = ticks;
    release(&tickslock);

    if (copyout2(addr, (char *)&t, sizeof(t)) < 0)
        return -1;

    return ret;
}
