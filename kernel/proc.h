#ifndef PROC_H
#define PROC_H

#include "types.h"

// RISC-V 64位通用寄存器存档
// 我们需要保存除了 x0 (zero) 以外的所有寄存器
struct trapframe {
    /* 0 */ uint64 kernel_satp;   // 内核页表 (如果是用户态陷入才需要，第一阶段可选)
    /* 8 */ uint64 kernel_sp;     // 内核栈顶
    /* 16 */ uint64 kernel_trap;  // 处理函数的地址 (usertrap)
    /* 24 */ uint64 epc;          // 保存的程序计数器 (sepc)
    /* 32 */ uint64 kernel_hartid;// 内核核心 ID
    
    // x1 (ra) - x31
    uint64 ra;
    uint64 sp;
    uint64 gp;
    uint64 tp;
    uint64 t0;
    uint64 t1;
    uint64 t2;
    uint64 s0;
    uint64 s1;
    uint64 a0;
    uint64 a1;
    uint64 a2;
    uint64 a3;
    uint64 a4;
    uint64 a5;
    uint64 a6;
    uint64 a7;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
    uint64 t3;
    uint64 t4;
    uint64 t5;
    uint64 t6;
};

#endif // PROC_H 