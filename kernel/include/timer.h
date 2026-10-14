#ifndef __TIMER_H
#define __TIMER_H

#include "spinlock.h"
#include "types.h"

#define MICRO_SEC 1000000
#define NANO_SEC  1000000000

extern struct spinlock tickslock;
extern uint ticks;

typedef unsigned long clock_t;

struct timeval {
    uint64 tv_sec;   // Seconds
    uint64 tv_usec;  // Microseconds [0, 1000000)
};

struct timespec {
    uint64 tv_sec;   // Seconds
    uint64 tv_nsec;  // Nanoseconds [0, 1000000000)
};

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
