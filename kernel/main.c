#include "defs.h"
#include "param.h"

void main()
{
    uartinit();

    printf("\n");
    printf("xv6 kernel is booting\n");

    kinit();
    kvminit();
    kvminithart();
    cpuinit();
    trap_init();
    plicinit();
    plicinithart();
    timerinit();

    printf("hart %d starting\n", cpuid());
    
    // 一阶段测试
    procinit();      // 初始化进程表锁
    test_proc_init(); // <--- 初始化我们的测试线程
    
    // 确保中断已关闭，调度器会根据需要开启
    intr_off(); 
    
    scheduler();     // <--- 开始调度，永不返回

    while(1) {
 
    }
}