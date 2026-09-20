#ifndef __SYSNUM_H
#define __SYSNUM_H

// ============================================================
// Linux / RISC-V compatible syscall numbers
// ============================================================

#define SYS_getcwd       17
#define SYS_dup          23
#define SYS_chdir        49
#define SYS_close        57
#define SYS_read         63
#define SYS_write        64
#define SYS_fstat        80
#define SYS_exit         93
#define SYS_kill         129
#define SYS_times        153
#define SYS_uname        160
#define SYS_getpid       172
#define SYS_shutdown     210


// ============================================================
// xv6 internal / custom syscall numbers
// add 1000 to avoid conflict with Linux ABI
// ============================================================

#define SYS_fork         1000
#define SYS_wait         1001
#define SYS_pipe         1002
#define SYS_exec         1003
#define SYS_sbrk         1004
#define SYS_sleep        1005
#define SYS_uptime       1006
#define SYS_open         1007
#define SYS_remove       1008
#define SYS_trace        1009
#define SYS_mkdir        1010
#define SYS_test_proc    1011
#define SYS_dev          1012
#define SYS_readdir      1013
#define SYS_sysinfo      1014
#define SYS_rename       1015

#endif