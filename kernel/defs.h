#ifndef DEFS_H
#define DEFS_H

#include "types.h"

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

#endif // DEFS_H
