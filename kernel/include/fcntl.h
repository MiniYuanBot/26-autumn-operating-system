#ifndef __FCNTL_H
#define __FCNTL_H

#define AT_FDCWD       -100

#define O_ACCMODE      0x000003
#define O_RDONLY       0x000000
#define O_WRONLY       0x000001
#define O_RDWR         0x000002
#define O_CREAT        0x000040 // different from linux
#define O_CREATE       O_CREAT
#define O_EXCL         0x000200
#define O_TRUNC        0x001000
#define O_APPEND       0x002000
#define O_DIRECTORY    0x200000

#endif