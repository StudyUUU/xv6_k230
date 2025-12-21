#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "riscv.h"

// Forward declarations
struct spinlock;

// uart.c
void uartinit(void);  
void uart_puts(char *s);
void printf(const char *fmt, ...);
int uart_getline(char *buf, int n);
void panic(const char *s);
void uartintr(void);           // 新增
void uart_intr_init(void);     // 新增

// kalloc.c
void* kalloc(void);
void kfree(void *);
void kinit(void);

// string.c
void* memset(void *dst, int c, uint n);
void* memmove(void *dst, const void *src, uint n);
void* memcpy(void *dst, const void *src, uint n);

// vm.c
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm);
void kvminit();
void kvminithart();
void check_mapping(uint64 va, uint64 expect_pa, int expect_perm, char *name);

// trap.c
void trap_init(void);
int devintr(void);             // 新增

// timer.c
void set_timer(uint64 stime_value);
void timerinit(void);

// proc.c
struct cpu*mycpu(void);
int cpuid();
void cpuinit();

// spinlock.c
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int);

#endif // DEFS_H
