#ifndef __MMAN_H
#define __MMAN_H

#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
// #define PROT_GROWSDOWN 0X01000000
// #define PROT_GROWSUP 0X02000000

#define MAP_FILE    0x0
#define MAP_SHARED  0x1
#define MAP_PRIVATE 0x2
// #define MAP_FAILED ((void *) -1)

#endif