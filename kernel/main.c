#include "defs.h"

void main()
{
    // 打印欢迎信息
    printf("\n");
    printf("--------------------------------\n");
    printf("xv6 on K230: Hello from S-mode!\n");
    printf("--------------------------------\n");

    kinit(); // 初始化物理内存管理
    printf("kinit success\n");


    void *p = kalloc();
    if(p){
        printf("kalloc success,address = %p\n", p);
        // 这里还没有 printf 打印地址，但只要不为 NULL 就行
        printf("got a page!\n");
        kfree(p);
    } else {
        printf("kalloc failed\n");
    }

    char cmd[CMD_BUF_SIZE];
    while(1) {
        printf("cmd: ");
        uart_getline(cmd, sizeof(cmd));
        printf("You entered: %s\n", cmd);
    }
    
}