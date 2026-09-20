#ifndef __TIMER_H
#define __TIMER_H

#include "spinlock.h"
#include "types.h"

extern struct spinlock tickslock;
extern uint ticks;

typedef unsigned long clock_t;

struct tms {
    clock_t tms_utime;  /* user time */
    clock_t tms_stime;  /* system time */
    clock_t tms_cutime; /* user time of children */
    clock_t tms_cstime; /* system time of children */
};

void timerinit();
void set_next_timeout();
void timer_tick();

#endif
