#ifndef PROC_H
#define PROC_H

#include "types.h"
#include "spinlock.h"
#include "param.h"

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

struct trapframe {
  uint64 kernel_satp;    // 0
  uint64 kernel_sp;      // 8
  uint64 kernel_trap;    // 16
  uint64 epc;            // 24
  uint64 kernel_hartid;  // 32

  uint64 ra;             // 40
  uint64 sp;             // 48
  uint64 gp;             // 56
  uint64 tp;             // 64
  uint64 t0;             // 72
  uint64 t1;             // 80
  uint64 t2;             // 88
  uint64 s0;             // 96
  uint64 s1;             // 104
  uint64 a0;             // 112  (注意：uservec 先用 sscratch 保存 a0，再写回这里)
  uint64 a1;             // 120
  uint64 a2;             // 128
  uint64 a3;             // 136
  uint64 a4;             // 144
  uint64 a5;             // 152
  uint64 a6;             // 160
  uint64 a7;             // 168
  uint64 s2;             // 176
  uint64 s3;             // 184
  uint64 s4;             // 192
  uint64 s5;             // 200
  uint64 s6;             // 208
  uint64 s7;             // 216
  uint64 s8;             // 224
  uint64 s9;             // 232
  uint64 s10;            // 240
  uint64 s11;            // 248
  uint64 t3;             // 256
  uint64 t4;             // 264
  uint64 t5;             // 272
  uint64 t6;             // 280
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  char name[16];                // Process name (debugging)
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