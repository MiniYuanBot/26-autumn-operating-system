#ifndef __PARAM_H
#define __PARAM_H

#define NPROC        50                 // maximum number of processes
#define NCPU          2                 // maximum number of CPUs
#define NOFILE       16                 // open files per process
#define NVMA         16                 // virtual memory areas per process
#define NFILE       100                 // open files per system
#define NINODE       50                 // maximum number of active i-nodes
#define NDEV         10                 // maximum major device number
#define ROOTDEV       1                 // device number of file system root disk
#define MAXARG       32                 // max exec arguments
#define MAXOPBLOCKS  10                 // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)    // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)    // size of disk block cache
#define FSSIZE       1000               // size of file system in blocks
#define MAXPATH      260                // maximum file path name
#define INTERVAL     (390000000 / 200)  // timer interrupt interval
#ifdef QEMU
#define TIMEBASE_FREQ 10000000ULL  // QEMU timebase frequency
#else
#define TIMEBASE_FREQ 7800000ULL   // K210 timebase frequency
#endif

#endif