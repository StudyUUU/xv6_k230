// main.c
#include <stdint.h>

// 声明串口输出函数 (在 uart.c)
void uart_putc(char c);
void uart_puts(char *s);

void main() {
    // 打印欢迎信息
    uart_puts("\n");
    uart_puts("--------------------------------\n");
    uart_puts("xv6 on K230: Hello from S-mode!\n");
    uart_puts("--------------------------------\n");

    // 死循环
    while(1) {
        // 这里可以加一个简单的回显测试
        // int c = uart_getc();
        // uart_putc(c);
    }
}