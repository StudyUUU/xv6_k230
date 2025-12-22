#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "riscv.h"

// Forward declarations
struct spinlock;
struct context;
struct proc;

// sbi.c - SBI 接口
void sbi_shutdown(void);
void sbi_reboot(void);

// uart.c - 串口驱动
void uartinit(void);
void uart_puts(char *s);
void printf(const char *fmt, ...);
int uartgetc(void);
void panic(const char *s);
void uartintr(void);

// kalloc.c - 物理内存分配
void* kalloc(void);
void kfree(void *);
void kinit(void);

// string.c - 字符串操作
void* memset(void *dst, int c, uint n);
void* memmove(void *dst, const void *src, uint n);
void* memcpy(void *dst, const void *src, uint n);

// vm.c - 虚拟内存管理
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm);
void kvminit(void);
void kvminithart(void);
void check_mapping(uint64 va, uint64 expect_pa, int expect_perm, char *name);
void kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, uint64 perm);
pagetable_t proc_pagetable(struct proc *p);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);
void uvmfree(pagetable_t pagetable, uint64 sz);

// trap.c - 陷阱处理
void trap_init(void);
int devintr(void);
void usertrap(void);

// timer.c - 定时器
void set_timer(uint64 stime_value);
void timerinit(void);

// proc.c - 进程管理
struct cpu* mycpu(void);
int cpuid(void);
void cpuinit(void);
void push_off(void);
void pop_off(void);
void proc_mapstacks(pagetable_t kpgtbl);
void procinit(void);
void test_proc_init(void);
void scheduler(void);

// spinlock.c - 自旋锁
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);

// plic.c - 平台级中断控制器
void plicinit(void);
void plicinithart(void);
int plic_claim(void);
void plic_complete(int);

// swtch.S - 上下文切换
void swtch(struct context*, struct context*);

// trampoline.S - 用户态/内核态切换
extern char trampoline[];  // trampoline 页的起始地址
extern char uservec[];     // 用户态陷阱入口
extern char userret[];     // 返回用户态的代码

#endif // DEFS_H
