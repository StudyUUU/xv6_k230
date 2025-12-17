#include "defs.h"
#include "memlayout.h"

extern char etext[]; 

void main()
{
    // 打印欢迎信息
    printf("\n");
    printf("--------------------------------\n");
    printf("xv6 on K230: Hello from S-mode!\n");
    printf("--------------------------------\n");
    
    // 2. 初始化物理内存分配器 (必须第一步做)
    kinit();
    printf("kinit success\n");


    // 3. 构建内核页表
    printf("kvminit: creating kernel page table...\n");
    kvminit();
    printf("kvminit success\n");

    // 4. 开启 MMU
    // 这一步执行完，如果没有死机，说明恒等映射成功了
    printf("kvminithart: enabling MMU...\n");
    kvminithart();

    // 5. 验证
    // 这条打印语句发出的数据的物理地址是 UART0
    // 但 CPU 此时是通过查询页表找到 UART0 的
    printf("MMU is ON! System is running in virtual memory mode.\n");

    while (1)
    {
    
    }
}