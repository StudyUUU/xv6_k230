#ifndef DEFS_H
#define DEFS_H

// uart.c
void uart_putc(char c);
void uart_puts(char *s);
int uart_getc(void);

// kalloc.c
void* kalloc(void);
void kfree(void *);
void kinit(void);

#endif // DEFS_H
