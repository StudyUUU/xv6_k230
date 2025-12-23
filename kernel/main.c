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
    
    // intr_on();

    procinit();      // 初始化进程表锁
    userinit();
    scheduler();     // <--- 开始调度，永不返回

    while(1) {
 
    }
}