// main.c
#include <stdint.h>

// 声明串口输出函数 (在 uart.c)
void uart_putc(char c);
void uart_puts(char *s);
int uart_getc(void);

void main()
{
    // 打印欢迎信息
    uart_puts("\n");
    uart_puts("--------------------------------\n");
    uart_puts("xv6 on K230: Hello from S-mode!\n");
    uart_puts("--------------------------------\n");

    while (1)
    {
        /* code */
    }
    
}