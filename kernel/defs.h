#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "riscv.h"

// uart.c
#define CMD_BUF_SIZE 36
void printf(const char *fmt, ...);
int uart_getline(char *buf, int n);
void panic(const char *s);

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
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
void kvminit();
void kvminithart();
void check_mapping(uint64 va, uint64 expect_pa, int expect_perm, char *name);

// trap.c
void trap_init(void);

// timer.c
void set_timer(uint64 stime_value);
void timerinit(void);

#endif // DEFS_H
