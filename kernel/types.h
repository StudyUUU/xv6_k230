#ifndef TYPES_H
#define TYPES_H

// 只在 C 代码中定义类型，汇编代码跳过
#ifndef __ASSEMBLER__

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

// 添加有符号类型
typedef char int8;
typedef short int16;
typedef int int32;
typedef long int64;

typedef uint64 pde_t;

#endif // __ASSEMBLER__

#endif // TYPES_H