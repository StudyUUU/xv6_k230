#ifndef PROC_H
#define PROC_H

#include "types.h"

// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
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
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 2. 进程控制块 (目前仅包含核心字段)
struct proc {
  struct spinlock lock;

  // p->lock 必须在修改 state 时持有
  enum procstate state;        // 进程状态
  void *chan;                  // 如果非空，表示正在等待该地址 (sleep用)
  int killed;                  // 如果非0，表示已被 kill
  int xstate;                  // 退出状态
  int pid;                     // 进程ID

  // 这里的 kstack 是虚拟地址
  uint64 kstack;               // 内核栈地址
  struct context context;      // swtch() 在这里保存寄存器
  
  // char name[16];            // 进程名 (可选，方便调试)
};


// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

#endif // PROC_H 