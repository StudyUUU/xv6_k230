#ifndef DEFS_H
#define DEFS_H

// uart.c
#define CMD_BUF_SIZE 36
void printf(const char *fmt, ...);
int uart_getline(char *buf, int n);
void panic(const char *s);

// kalloc.c
void* kalloc(void);
void kfree(void *);
void kinit(void);

#endif // DEFS_H
