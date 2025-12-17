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

    test_vm();

    while (1)
    {
        /* code */
    }
    
    
}