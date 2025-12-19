#include "defs.h"

// 声明 start.c 中的同步变量
extern volatile int started;

void main()
{
    int hartid = cpuid();
    printf(">>> Hart %d entered main() <<<\n", hartid);
    
    if (hartid == 0) {
        // ==================== 主核（Hart 0）====================
        printf("\n");
        printf("--------------------------------\n");
        printf("xv6 on K230: Hello from S-mode!\n");
        printf("--------------------------------\n");
        
        kinit();
        printf("kinit success\n");

        kvminit();
        printf("kvminit success\n");

        kvminithart();
        printf("MMU is ON!\n");
        
        cpuinit();
        printf("cpuinit success (hartid=%d)\n", cpuid());
        
        trap_init();
        printf("trap_init success\n");

        timerinit();
        printf("timerinit success. interval = %d\n", CLOCK_INTERVAL);

        intr_on();
        printf("interrupts enabled on hart 0\n");

        // 通知其他核心：主核初始化完成
        __sync_synchronize();
        started = 1;
        __sync_synchronize();

        printf("\nSystem ready. Hart 0 running.\n");
    } 
    else {
        // ==================== 从核（Hart 1+）====================
        // 等待主核完成共享资源初始化
        while (started == 0)
            ;
        __sync_synchronize();
        
        // 每个核心独立初始化自己的状态
        kvminithart();      // 启用 MMU（使用主核创建的页表）
        cpuinit();          // 初始化本 CPU 的 struct cpu
        trap_init();        // 设置中断向量
        timerinit();        // 设置定时器
        intr_on();          // 开启中断
        
        printf("Hart %d started and ready\n", hartid);
    }

    // 所有核心都进入空闲循环
    while (1)
        ;
}
