#include "defs.h"

void main()
{
    // 打印欢迎信息
    uart_puts("\n");
    uart_puts("--------------------------------\n");
    uart_puts("xv6 on K230: Hello from S-mode!\n");
    uart_puts("--------------------------------\n");

    kinit(); // 初始化物理内存管理
    uart_puts("kinit success\n");


    void *p = kalloc();
    if(p){
        uart_puts("kalloc success,address = %p\n");
        // 这里还没有 printf 打印地址，但只要不为 NULL 就行
        uart_puts("got a page!\n");
        kfree(p);
    } else {
        uart_puts("kalloc failed\n");
    }

    while(1);
}