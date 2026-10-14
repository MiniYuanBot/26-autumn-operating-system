#ifndef __TYPES_H
#define __TYPES_H

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef unsigned short wchar;

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long  uint64;

typedef unsigned long uintptr_t;
typedef uint64 pde_t;

typedef signed char    int8;
typedef short          int16;
typedef int            int32;
typedef long           int64;

#define XV6_INT_MAX ((int)(~0U >> 1))
#define XV6_INT_MIN (-XV6_INT_MAX - 1)
#define XV6_LONG_MAX ((int)(~0U >> 1))
#define XV6_LONG_MIN (-XV6_LONG_MAX - 1)

// #define NULL ((void *)0)
#define NULL 0

#define readb(addr) (*(volatile uint8 *)(addr))
#define readw(addr) (*(volatile uint16 *)(addr))
#define readd(addr) (*(volatile uint32 *)(addr))
#define readq(addr) (*(volatile uint64 *)(addr))

#define writeb(v, addr)                      \
    {                                        \
        (*(volatile uint8 *)(addr)) = (v); \
    }
#define writew(v, addr)                       \
    {                                         \
        (*(volatile uint16 *)(addr)) = (v); \
    }
#define writed(v, addr)                       \
    {                                         \
        (*(volatile uint32 *)(addr)) = (v); \
    }
#define writeq(v, addr)                       \
    {                                         \
        (*(volatile uint64 *)(addr)) = (v); \
    }

#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
